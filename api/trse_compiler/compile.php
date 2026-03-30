<?php
/**
 * TRSE compile API — same request shape as api/kickass/compile.php
 *
 * Requires `trse` on PATH or Docker image `trse:cli` (see TRSE_MODE below).
 *
 * JSON body:
 *   sessionID   — required, alphanumeric (isolates /tmp/trse-api-{id})
 *   updates     — required, array of { path, data } (UTF-8 text or data:base64,... for binaries)
 *   buildStep   — required:
 *       project — path to .trse file relative to session root (e.g. "project.trse")
 *       path    — main source .ras or .tru relative to session root (e.g. "files/main.ras")
 *       assemble — optional bool, default true (false = compile to .asm only)
 *
 * Environment variables:
 *   TRSE_MODE = docker | native
 *   TRSE_DOCKER_IMAGE = trse:cli
 *   TRSE_NATIVE_BIN = /opt/trse/bin/trse
 *   TRSE_SETTINGS = /etc/trse/trse.ini   (copied into session as trse.ini)
 *   TRSE_UNITS_STOCK = /path/to/TRSE/units  (optional; copied into session/units/)
 *
 * Response (success):
 *   artifacts: [ { "name": "main.prg", "mime": "application/octet-stream", "data": "<base64>" }, ... ]
 *   log: string (compiler stdout/stderr)
 *
 * Response (failure):
 *   errors: [ { "line": 0, "msg": "...", "path": "" }, ... ]
 *   log: string (optional)
 */

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
header('Access-Control-Max-Age: 86400');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

header('Content-Type: application/json; charset=utf-8');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    exit(json_encode(['error' => 'Method not allowed']));
}

// --- Configuration (adjust for your server) ---
$TRSE_MODE = getenv('TRSE_MODE') ?: 'docker'; // docker | native
$TRSE_DOCKER_IMAGE = getenv('TRSE_DOCKER_IMAGE') ?: 'trse:cli';
$TRSE_NATIVE_BIN = getenv('TRSE_NATIVE_BIN') ?: 'trse';
/** Host path to trse.ini; copied into session as trse.ini when set (so CLI always finds settings). */
$TRSE_SETTINGS_HOST = getenv('TRSE_SETTINGS') ?: '/etc/trse/trse.ini';
/** If set, recursively copy this directory into session as `units/` before writes (e.g. stock TRSE units). */
$TRSE_UNITS_STOCK = getenv('TRSE_UNITS_STOCK') ?: '';

$input = file_get_contents('php://input');
$data = json_decode($input, true);

if (json_last_error() !== JSON_ERROR_NONE) {
    http_response_code(400);
    exit(json_encode(['error' => 'Invalid JSON: ' . json_last_error_msg()]));
}

if (!isset($data['buildStep']) || !isset($data['updates']) || !isset($data['sessionID'])) {
    http_response_code(400);
    exit(json_encode(['error' => 'Missing required fields: buildStep, updates, or sessionID']));
}

$buildStep = $data['buildStep'];
$updates = $data['updates'];
$sessionID = $data['sessionID'];

if (!preg_match('/^[a-zA-Z0-9_-]{8,128}$/', $sessionID)) {
    http_response_code(403);
    exit(json_encode(['error' => 'Invalid sessionID (use 8–128 chars [a-zA-Z0-9_-])']));
}

if (!isset($buildStep['path']) || !isset($buildStep['project'])) {
    http_response_code(400);
    exit(json_encode(['error' => 'buildStep must include project (.trse) and path (main .ras/.tru)']));
}

if (!is_array($updates) || empty($updates)) {
    http_response_code(400);
    exit(json_encode(['error' => 'updates must be a non-empty array']));
}

$result = compileTrse($buildStep, $updates, $sessionID, [
    'mode' => $TRSE_MODE,
    'docker_image' => $TRSE_DOCKER_IMAGE,
    'native_bin' => $TRSE_NATIVE_BIN,
    'settings_host' => $TRSE_SETTINGS_HOST,
    'units_stock' => $TRSE_UNITS_STOCK,
]);

echo json_encode($result, JSON_UNESCAPED_UNICODE);

// -----------------------------------------------------------------------------

function compileTrse($buildStep, $updates, $sessionID, $cfg)
{
    $sessionDir = '/tmp/trse-api-' . $sessionID;

    if (!is_dir($sessionDir) && !mkdir($sessionDir, 0700, true)) {
        return ['errors' => [['line' => 0, 'msg' => "Failed to create session directory: {$sessionDir}", 'path' => '']]];
    }

    if (!is_writable($sessionDir)) {
        return ['errors' => [['line' => 0, 'msg' => "Session directory not writable: {$sessionDir}", 'path' => '']]];
    }

    // Optional: seed stock units (global includes) from TRSE install
    if ($cfg['units_stock'] !== '' && is_dir($cfg['units_stock'])) {
        trseRecursiveCopy($cfg['units_stock'], $sessionDir . '/units');
    }

    foreach ($updates as $update) {
        if (!isset($update['path']) || !isset($update['data'])) {
            return ['errors' => [['line' => 0, 'msg' => 'Invalid update structure', 'path' => '']]];
        }
        $rel = $update['path'];
        $raw = $update['data'];
        if (is_string($raw) && strpos($raw, 'data:base64,') === 0) {
            $raw = base64_decode(substr($raw, 12), true);
            if ($raw === false) {
                return ['errors' => [['line' => 0, 'msg' => 'Invalid base64 data', 'path' => $rel]]];
            }
        }
        $w = trseSafeWrite($sessionDir, $rel, $raw);
        if ($w !== true) {
            return ['errors' => [['line' => 0, 'msg' => $w, 'path' => $rel]]];
        }
    }

    $projectFile = $buildStep['project'];
    $mainFile = $buildStep['path'];
    $assemble = isset($buildStep['assemble']) ? (bool)$buildStep['assemble'] : true;

    if (!trseIsSafeRelative($projectFile) || !trseIsSafeRelative($mainFile)) {
        return ['errors' => [['line' => 0, 'msg' => 'Invalid project or path', 'path' => '']]];
    }

    if (!is_file($sessionDir . '/' . $projectFile)) {
        return ['errors' => [['line' => 0, 'msg' => "Project file not found: {$projectFile}", 'path' => $projectFile]]];
    }
    if (!is_file($sessionDir . '/' . $mainFile)) {
        return ['errors' => [['line' => 0, 'msg' => "Main source not found: {$mainFile}", 'path' => $mainFile]]];
    }

    // Ensure CLI has a settings file (trc.cpp loads AppData or explicit settings=)
    $settingsArg = 'settings=trse.ini';
    if (is_readable($cfg['settings_host'])) {
        if (!copy($cfg['settings_host'], $sessionDir . '/trse.ini')) {
            return ['errors' => [['line' => 0, 'msg' => 'Could not copy TRSE settings to session', 'path' => 'trse.ini']]];
        }
    } elseif (!is_file($sessionDir . '/trse.ini')) {
        return ['errors' => [['line' => 0, 'msg' => 'No trse.ini: set TRSE_SETTINGS to a valid host file or upload trse.ini in updates', 'path' => 'trse.ini']]];
    }

    $args = [
        'op=project',
        'project=' . $projectFile,
        'input_file=' . $mainFile,
        $settingsArg,
        $assemble ? 'assemble=yes' : 'assemble=no',
    ];

    $cmd = trseBuildCommand($cfg, $sessionDir, $args);
    $log = '';
    $code = 0;
    exec($cmd . ' 2>&1', $out, $code);
    $log = implode("\n", $out);

    if ($code !== 0) {
        return [
            'errors' => trseParseErrors($log),
            'log' => $log,
        ];
    }

    $artifacts = trseCollectArtifacts($sessionDir, $mainFile, $projectFile);

    return [
        'artifacts' => $artifacts,
        'log' => $log,
    ];
}

function trseIsSafeRelative($path)
{
    $path = str_replace('\\', '/', $path);
    if ($path === '' || $path[0] === '/') {
        return false;
    }
    if (preg_match('#(^|/)\.\.(/|$)#', $path)) {
        return false;
    }
    return true;
}

/** @return true|string error message */
function trseSafeWrite($baseDir, $relativePath, $contents)
{
    if (!trseIsSafeRelative($relativePath)) {
        return 'Invalid path';
    }
    $relativePath = str_replace('\\', '/', $relativePath);
    $full = $baseDir . '/' . $relativePath;
    $dir = dirname($full);
    if (!is_dir($dir) && !mkdir($dir, 0700, true)) {
        return 'Failed to create directory';
    }
    $realBase = realpath($baseDir);
    $realFile = realpath($dir);
    if ($realBase === false || $realFile === false || strpos($realFile, $realBase) !== 0) {
        return 'Path escapes session directory';
    }
    if (file_put_contents($full, $contents) === false) {
        return 'Write failed';
    }
    return true;
}

function trseRecursiveCopy($src, $dst)
{
    if (!is_dir($dst)) {
        mkdir($dst, 0700, true);
    }
    $it = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($src, RecursiveDirectoryIterator::SKIP_DOTS),
        RecursiveIteratorIterator::SELF_FIRST
    );
    foreach ($it as $item) {
        $target = $dst . DIRECTORY_SEPARATOR . $it->getSubPathName();
        if ($item->isDir()) {
            if (!is_dir($target)) {
                mkdir($target, 0700, true);
            }
        } else {
            copy($item, $target);
        }
    }
}

function trseBuildCommand($cfg, $sessionDir, $args)
{
    $escapedArgs = array_map('escapeshellarg', $args);

    if ($cfg['mode'] === 'docker') {
        return escapeshellarg('docker') . ' run --rm -v ' . escapeshellarg($sessionDir . ':/work')
            . ' -w ' . escapeshellarg('/work') . ' ' . escapeshellarg($cfg['docker_image'])
            . ' trse -cli ' . implode(' ', $escapedArgs);
    }

    $bin = escapeshellcmd($cfg['native_bin']);
    $wd = escapeshellarg($sessionDir);
    return 'cd ' . $wd . ' && ' . $bin . ' -cli ' . implode(' ', $escapedArgs);
}

function trseParseErrors($log)
{
    $errors = [];
    foreach (explode("\n", $log) as $line) {
        $line = trim($line);
        if ($line === '') {
            continue;
        }
        if (preg_match('/\.ras[:(]\s*(\d+)\s*[):]/i', $line, $m)) {
            $errors[] = ['line' => (int)$m[1], 'msg' => $line, 'path' => ''];
        } elseif (preg_match('/error|fatal|Error/i', $line)) {
            $errors[] = ['line' => 0, 'msg' => $line, 'path' => ''];
        }
    }
    if (empty($errors) && trim($log) !== '') {
        $errors[] = ['line' => 0, 'msg' => trim($log), 'path' => ''];
    }
    return $errors;
}

/**
 * Collect build products from session dir (flat list).
 * Primary outputs depend on project .trse (output_type, system).
 */
function trseCollectArtifacts($sessionDir, $mainFile, $projectFile)
{
    $base = pathinfo($mainFile, PATHINFO_FILENAME);
    $exts = [
        'prg', 'd64', 'd81', 'crt', 'tap', 't64', 'gb', 'gbc', 'nes', 'asm',
        'vs', 'sym',
    ];
    $out = [];
    $seen = [];

    foreach ($exts as $ext) {
        $f = $sessionDir . '/' . $base . '.' . $ext;
        if (is_file($f)) {
            $seen[$f] = true;
            $out[] = trseArtifact($f);
        }
    }

    foreach (glob($sessionDir . '/*.{prg,d64,crt,tap,gb,nes}', GLOB_BRACE) ?: [] as $f) {
        if (empty($seen[$f])) {
            $seen[$f] = true;
            $out[] = trseArtifact($f);
        }
    }
    $filesDir = $sessionDir . '/files';
    if (is_dir($filesDir)) {
        foreach (glob($filesDir . '/*.{prg,d64,crt}', GLOB_BRACE) ?: [] as $f) {
            if (empty($seen[$f])) {
                $seen[$f] = true;
                $out[] = trseArtifact($f);
            }
        }
    }

    return $out;
}

function trseArtifact($fullPath)
{
    $name = basename($fullPath);
    $data = file_get_contents($fullPath);
    return [
        'name' => $name,
        'mime' => 'application/octet-stream',
        'data' => base64_encode($data),
    ];
}
