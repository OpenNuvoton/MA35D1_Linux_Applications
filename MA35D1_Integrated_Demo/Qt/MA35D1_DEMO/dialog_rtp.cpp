#include "mainwindow.h"
#include "dialog_rtp.h"
#include "ui_dialog_rtp.h"



Dialog_RTP::Dialog_RTP(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog_RTP)
{
    ui->setupUi(this);

    nums.push_back(ui->pushButton_1);
    nums.push_back(ui->pushButton_2);
    nums.push_back(ui->pushButton_3);
    nums.push_back(ui->pushButton_4);
    nums.push_back(ui->pushButton_5);
    nums.push_back(ui->pushButton_6);
    nums.push_back(ui->pushButton_7);
    nums.push_back(ui->pushButton_8);
    nums.push_back(ui->pushButton_9);

    bStop = true;
}

Dialog_RTP::~Dialog_RTP()
{
    delete ui;
}

void Dialog_RTP::keyPressEvent(QKeyEvent *event){
    int num = event->key() - Qt::Key_0;
    if(num == 0){
        ui->label_status->setText("TRIGGERING");
        return;
    }
    BtnSts(prev_num, false);
    BtnSts(num, true);
    prev_num = num;
    ui->label_status->setText("STANDBY");

}

void Dialog_RTP::BtnSts(int n, bool sts){
    if(n > 9 || n < 1)
            return;
    if(sts)
        nums[n-1]->setStyleSheet("font: 60pt;");

    else
        nums[n-1]->setStyleSheet("font: 20pt");
}


void Dialog_RTP::on_pushButton_close_clicked()
{
    QProcess::execute("/opt/ma35d1_demo.sh b"); //STOP RTP
    delete this;
}

void Dialog_RTP::on_pushButton_RTP_start_clicked()
{
    if(bStop == true){
        QProcess::execute("/opt/ma35d1_demo.sh a"); //START RTP
        bStop = false;
        ui->pushButton_RTP_start->setText("RTP KWS STOP");
    }
    else{
        QProcess::execute("/opt/ma35d1_demo.sh b"); //STOP RTP
        bStop = true;
        ui->pushButton_RTP_start->setText("RTP KWS START");
    }
}
