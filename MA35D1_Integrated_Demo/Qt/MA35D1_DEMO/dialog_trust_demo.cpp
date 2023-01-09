#include "mainwindow.h"
#include "dialog_trust_demo.h"
#include "ui_dialog_trust_demo.h"
#include <QProcess>
#include <QDebug>

Dialog_Trust_Demo::Dialog_Trust_Demo(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog_Trust_Demo)
{
    ui->setupUi(this);
    pWnd_main->b_dlg_trust_demo_on = true;
    QPixmap pixmap("/opt/Trust_pics/process1.png");
    QPixmap tmp = pixmap.scaled(ui->label_process->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    qTimer = new QTimer();
    connect(this->qTimer,SIGNAL(timeout()),SLOT(slot_ShowPicture()));
    ui->label_process->setPixmap(tmp);
    qTimer->start(1000);

}

Dialog_Trust_Demo::~Dialog_Trust_Demo()
{
    pWnd_main->b_dlg_trust_demo_on = false;
    delete ui;
}

void Dialog_Trust_Demo::on_pushButton_close_clicked()
{
    delete this;
}

void Dialog_Trust_Demo::on_pushButton_CI_select_clicked()
{
    this->setStyleSheet("QFileDialog QWidget {font: 16pt;}");
    QStringList arg_secure_demo = QFileDialog::getOpenFileNames(this, tr("Open File"),"/opt/optee_demo",tr("CI Files (*.txt)"));
    if(!arg_secure_demo.empty()){
        ui->lineEdit_CI_select->clear();
        ui->lineEdit_CI_select->setText(arg_secure_demo.join(""));
    }
}

void Dialog_Trust_Demo::on_pushButton_execute_clicked()
{
    char buf[256];
    FILE * fp;
    QPixmap p_pass(":/Pics/Pics/PASS.png");
    QPixmap p_fail(":/Pics/Pics/FAIL.png");
    QString qs_cmd = "/opt/optee_demo/bin/ca/optee_example_secure_demo";
    int rtn;

    qs_cmd += " " + ui->lineEdit_CI_select->displayText();

    if ((fp = popen(qs_cmd.toStdString().c_str(), "r")) == NULL)
    {
        qDebug()<<"allocate memory failed!";
    }

    while (fgets(buf, 256, fp) != NULL) qDebug()<<"result:"<<buf;
    rtn = pclose(fp);

    if (rtn != 0)
    {
        ui->label_result->setPixmap(p_fail.scaled(160,120,Qt::KeepAspectRatio));
        return;
    }

    ui->label_result->setPixmap(p_pass.scaled(160,120,Qt::KeepAspectRatio));
}

void Dialog_Trust_Demo::slot_ShowPicture(){
    ++imgNum;
    QString path = "/opt/Trust_pics/process" + QString::number(imgNum) + ".png";
    QPixmap pixmap(path);
    QPixmap tmp = pixmap.scaled(ui->label_process->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->label_process->setPixmap(tmp);
    imgNum = (imgNum == 9) ? 0 : imgNum++;
}

void Dialog_Trust_Demo::on_pushButton_clicked()
{
    if(!this->bStop){
        qTimer->stop();
        bStop = true;
        ui->pushButton->setText("PLAY");
    }
    else{
        qTimer->start();
        bStop = false;
        ui->pushButton->setText("STOP");
    }
}
