#include "dialogsplash.h"
#include "ui_dialogsplash.h"
#include <QTimer>
#include <QPalette>

DialogSplash::DialogSplash(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogSplash)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    QStringList texts = QStringList() << "Better than it sounds"<<
          "Make our dreams come true"<<
          "Less powerful than Malboge, but definitely cuter"<<
          "When your only tool is TRSE, all problems start looking like nails."<<
          "I have only made this code longer because I have not had the time to make it shorter"<<
          "I would prefer an intelligent hell to a stupid paradise"<<
          "Kind bytes don't cost much. Yet they accomplish much." <<
          "You always admire what you really don't understand."<<
          "Everything else is something different" <<
          "Kind of OK" <<
          "Drink and be Refreshed "<<
          "Wherever you go, there you are"<<
          "Non fui, fui, non sum, non curo"<<
          "Not the worst choice I guess"<<
          "your mother would be proud. Probably." <<
          "What is the worst that could happen?";                               ;



    ui->lblText->setText(texts[rand()%texts.count()]);
    // -- Bones: Trying to fix the off-center spash screen on windows
#ifdef _WIN32
    const QRect screen = parent->geometry();
        this->move( screen.center() );
#endif
}

DialogSplash::~DialogSplash()
{
    delete ui;
}

void DialogSplash::Init()
{
    QTimer::singleShot(m_seconds*1000, this, SLOT(CloseMe()));
    QPalette pal = palette();

    // set black background
    pal.setColor(QPalette::Window, QColor(45,44,146));
    setAutoFillBackground(true);

    setPalette(pal);
}

void DialogSplash::CloseMe()
{
    close();
}
