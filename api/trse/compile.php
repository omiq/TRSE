<?php
/**
 * TRSE compile API — same request shape as api/kickass/compile.php
 *
 * JSON body:
 *   sessionID, updates[], buildStep { project, path, assemble? }
 *
 * Environment:
 *   TRSE_MODE, TRSE_DOCKER_IMAGE, TRSE_NATIVE_BIN, TRSE_SETTINGS, TRSE_UNITS_STOCK
 *   TRSE_DEFAULTS_INI — path to trse.defaults.ini (default: __DIR__/trse.defaults.ini)
 *
 * API host spec:
 *   - TRS80COCO / DRAGON: session project.trse is rewritten to output_type = bin (skip MAME imgtool).
 *   - trse.ini: strip # lines; merge defaults (assembler_6809 = OrgAsm); host TRSE_SETTINGS copied first.
 *
 * Requires TRSE with SystemTRS80CoCo::PostProcess skipping imgtool when output_type=bin (upstream TRSE).
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

$TRSE_MODE = getenv('TRSE_MODE') ?: 'docker';
$TRSE_DOCKER_IMAGE = getenv('TRSE_DOCKER_IMAGE') ?: 'trse:cli';
$TRSE_NATIVE_BIN = getenv('TRSE_NATIVE_BIN') ?: 'trse';
$TRSE_SETTINGS_HOST = getenv('TRSE_SETTINGS') ?: '/etc/trse/trse.ini';
$TRSE_UNITS_STOCK = getenv('TRSE_UNITS_STOCK') ?: '';
$TRSE_DEFAULTS_INI = getenv('TRSE_DEFAULTS_INI') ?: (__DIR__ . '/trse.defaults.ini');

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
    'defaults_ini' => $TRSE_DEFAULTS_INI,
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

    $settingsPath = $sessionDir . '/trse.ini';
    $settingsArg = 'settings=trse.ini';

    if (is_readable($cfg['settings_host'])) {
        if (!copy($cfg['settings_host'], $settingsPath)) {
            return ['errors' => [['line' => 0, 'msg' => 'Could not copy TRSE settings to session', 'path' => 'trse.ini']]];
        }
    } elseif (!is_file($settingsPath)) {
        return ['errors' => [['line' => 0, 'msg' => 'No trse.ini: set TRSE_SETTINGS or upload trse.ini in updates', 'path' => 'trse.ini']]];
    }

    trseStripHashCommentsIni($settingsPath);
    trseMergeIniDefaults($settingsPath, $cfg['defaults_ini']);
    trseApiForceCoCoDragonBin($sessionDir, $projectFile);

    $args = [
        'op=project',
        'project=' . $projectFile,
        'input_file=' . $mainFile,
        $settingsArg,
        $assemble ? 'assemble=yes' : 'assemble=no',
    ];

    $cmd = trseBuildCommand($cfg, $sessionDir, $args);
    exec($cmd . ' 2>&1', $out, $code);
    $log = implode("\n", $out);

    if ($code !== 0) {
        return [
            'errors' => trseParseErrors($log),
            'log' => $log,
        ];
    }

    return [
        'artifacts' => trseCollectArtifacts($sessionDir, $mainFile, $projectFile),
        'log' => $log,
    ];
}

/** TRSE CIniFile treats # as invalid lines — remove # comment lines. */
function trseStripHashCommentsIni($path)
{
    $raw = file_get_contents($path);
    if ($raw === false) {
        return;
    }
    $lines = preg_split('/\r\n|\r|\n/', $raw);
    $kept = [];
    foreach ($lines as $line) {
        $t = trim($line);
        if ($t !== '' && isset($t[0]) && $t[0] === '#') {
            continue;
        }
        $kept[] = $line;
    }
    file_put_contents($path, implode("\n", $kept) . "\n");
}

function trseIniParseKeys($content)
{
    $keys = [];
    foreach (preg_split('/\r\n|\r|\n/', $content) as $line) {
        $line = trim($line);
        if ($line === '' || (isset($line[0]) && ($line[0] === ';' || $line[0] === '#'))) {
            continue;
        }
        if (strpos($line, '=') === false) {
            continue;
        }
        $k = strtolower(trim(explode('=', $line, 2)[0]));
        $keys[$k] = true;
    }
    return $keys;
}

/** Append lines from defaults for keys not already in session trse.ini */
function trseMergeIniDefaults($sessionIniPath, $defaultsPath)
{
    if (!is_readable($defaultsPath)) {
        return;
    }
    $sess = file_get_contents($sessionIniPath);
    if ($sess === false) {
        return;
    }
    $have = trseIniParseKeys($sess);
    $def = file_get_contents($defaultsPath);
    if ($def === false) {
        return;
    }
    $append = '';
    foreach (preg_split('/\r\n|\r|\n/', $def) as $line) {
        $t = trim($line);
        if ($t === '' || (isset($t[0]) && $t[0] === ';')) {
            continue;
        }
        if (strpos($line, '=') === false) {
            continue;
        }
        $k = strtolower(trim(explode('=', $line, 2)[0]));
        if (!isset($have[$k])) {
            $append .= trim($line) . "\n";
            $have[$k] = true;
        }
    }
    if ($append !== '') {
        file_put_contents($sessionIniPath, rtrim($sess) . "\n" . $append);
    }
}

/** API builds: flat .bin only (no MAME imgtool) for CoCo / Dragon. */
function trseApiForceCoCoDragonBin($sessionDir, $projectFile)
{
    $full = $sessionDir . '/' . $projectFile;
    $content = file_get_contents($full);
    if ($content === false) {
        return;
    }
    if (!preg_match('/^\s*system\s*=\s*(\S+)/mi', $content, $m)) {
        return;
    }
    $sys = strtoupper(trim($m[1]));
    if ($sys !== 'TRS80COCO' && $sys !== 'DRAGON') {
        return;
    }
    if (preg_match('/^\s*output_type\s*=/mi', $content)) {
        $content = preg_replace('/^\s*output_type\s*=\s*[^\r\n]*/mi', 'output_type = bin', $content);
    } else {
        $content = rtrim($content) . "\noutput_type = bin\n";
    }
    file_put_contents($full, $content);
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

/** @return true|string */
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

function trseCollectArtifacts($sessionDir, $mainFile, $projectFile)
{
    $base = pathinfo($mainFile, PATHINFO_FILENAME);
    $exts = [
        'prg', 'bin', 'd64', 'd81', 'crt', 'tap', 't64', 'gb', 'gbc', 'nes', 'asm', 'vs', 'sym',
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

    foreach (glob($sessionDir . '/*.{prg,bin,d64,crt,tap,gb,nes}', GLOB_BRACE) ?: [] as $f) {
        if (empty($seen[$f])) {
            $seen[$f] = true;
            $out[] = trseArtifact($f);
        }
    }
    $filesDir = $sessionDir . '/files';
    if (is_dir($filesDir)) {
        foreach (glob($filesDir . '/*.{prg,bin,d64,crt}', GLOB_BRACE) ?: [] as $f) {
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
