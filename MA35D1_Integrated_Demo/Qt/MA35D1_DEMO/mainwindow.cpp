#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLabel>
#include <QMovie>
#include <QDesktopWidget>

Dialog_Cam  *pWnd_cam;
MainWindow  *pWnd_main;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{   
    ui->setupUi(this);
    this->statusBar()->hide();
    if(Init() != 0){
        QMessageBox::critical(NULL,  "critical",  "Initial Failed!", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::Init(){
    dlg_cam = NULL;
    p_video_play = NULL;
    p_phone_gui = NULL;
    b_2d_ghc_on = false;
    b_dlg_cam_on = false;
    b_dlg_trust_demo_on = false;
    //Load RTP FW
    QProcess::execute("/opt/ma35d1_demo.sh 9");

    return 0;

init_error:
    return -1;
}

void MainWindow::mousePressEvent(QMouseEvent *event){
    if(p_video_play && p_video_play->state() == QProcess::Running){
        QProcess::execute("/opt/ma35d1_demo.sh 2");
    }
}

void MainWindow::pushButton_stsSet(bool sts){
    ui->pushButton_peoplecnt->setEnabled(sts);
    ui->pushButton_h264dec->setEnabled(sts);
    ui->pushButton_2d_ghc->setEnabled(sts);
    ui->pushButton_sip_phone->setEnabled(sts);
    ui->pushButton_rtp->setEnabled(sts);
    ui->pushButton_trust_demo->setEnabled(sts);
}

void MainWindow::slot_video_play_stop(){
    p_video_play = NULL;
    pushButton_stsSet(true);
    this->repaint();
    this->update();
    this->show();
}

void MainWindow::on_pushButton_h264dec_clicked()
{
        QStringList arguments;
        arguments << "/opt/ma35d1_demo.sh"<<"1";
        p_video_play = new QProcess(this);
        connect(p_video_play, SIGNAL(finished(int)), this, SLOT(slot_video_play_stop()));
        p_video_play->start("sh", arguments);
        pushButton_stsSet(false);
}

void MainWindow::on_pushButton_h264dec_pressed()
{
    ui->pushButton_h264dec->setIcon(QIcon(":Pics/Pics/Menu_button_Video_pressed"));
}

void MainWindow::on_pushButton_h264dec_released()
{
    ui->pushButton_h264dec->setIcon(QIcon(":Pics/Pics/Menu_button_Video"));
}

void MainWindow::on_pushButton_peoplecnt_clicked()
{
    if(b_dlg_cam_on) return;

    dlg_cam = new Dialog_Cam;
    dlg_cam->show();
}

void MainWindow::on_pushButton_peoplecnt_pressed()
{
   ui->pushButton_peoplecnt->setIcon(QIcon(":/Pics/Pics/Menu_button_Cam_Pressed.png"));
}

void MainWindow::on_pushButton_peoplecnt_released()
{
       ui->pushButton_peoplecnt->setIcon(QIcon(":/Pics/Pics/Menu_button_Cam.png"));
}

void MainWindow::on_pushButton_2d_ghc_clicked()
{
    this->hide();
    pushButton_stsSet(false);
    QProcess::execute("/opt/ma35d1_demo.sh 3");
    QStringList arguments;
}

void MainWindow::on_pushButton_2d_ghc_pressed()
{
    ui->pushButton_2d_ghc->setIcon(QIcon(":/Pics/Pics/Menu_button_2D_pressed.png"));
}

void MainWindow::on_pushButton_2d_ghc_released()
{
    ui->pushButton_2d_ghc->setIcon(QIcon(":/Pics/Pics/Menu_button_2D.png"));
}

void MainWindow::on_pushButton_trust_demo_clicked()
{
    if(b_dlg_trust_demo_on) return;

    dlg_trust_demo = new Dialog_Trust_Demo;
    dlg_trust_demo->show();
}

void MainWindow::on_pushButton_trust_demo_pressed()
{
    ui->pushButton_trust_demo->setIcon(QIcon(":/Pics/Pics/Menu_button_Security_pressed.png"));
}

void MainWindow::on_pushButton_trust_demo_released()
{
    ui->pushButton_trust_demo->setIcon(QIcon(":/Pics/Pics/Menu_button_Security.png"));
}


void MainWindow::on_pushButton_rtp_clicked()
{
    dlg_rtp = new Dialog_RTP;
    dlg_rtp->show();
}

void MainWindow::on_pushButton_rtp_pressed()
{
    ui->pushButton_rtp->setIcon(QIcon(":/Pics/Pics/Menu_button_KWS_pressed.png"));
}

void MainWindow::on_pushButton_rtp_released()
{
    ui->pushButton_rtp->setIcon(QIcon(":/Pics/Pics/Menu_button_KWS.png"));
}

void MainWindow::on_pushButton_sip_phone_clicked()
{
    QStringList arguments;
    arguments << "/opt/ma35d1_demo.sh"<<"5";
    p_phone_gui = new QProcess(this);
    connect(p_phone_gui, SIGNAL(finished(int)), this, SLOT(slot_baresip_stop()));
    p_phone_gui->start("sh", arguments);
    this->hide();
    pushButton_stsSet(false);
}

void MainWindow::slot_baresip_stop(){
    p_phone_gui = NULL;

    QProcess::execute("/opt/ma35d1_demo.sh 8");
    this->update();
    pushButton_stsSet(true);
    this->show();
}

void MainWindow::on_pushButton_sip_phone_pressed()
{
    ui->pushButton_sip_phone->setIcon(QIcon(":/Pics/Pics/Menu_button_VoIP_pressed.png"));
}

void MainWindow::on_pushButton_sip_phone_released()
{
    ui->pushButton_sip_phone->setIcon(QIcon(":/Pics/Pics/Menu_button_VoIP.png"));
}
