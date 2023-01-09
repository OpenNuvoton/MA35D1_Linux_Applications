#ifndef DIALOG_TRUST_DEMO_H
#define DIALOG_TRUST_DEMO_H

#include <QDialog>
#include <QFileDialog>
#include <stdio.h>
#include <QProcess>
#include <QTimer>

namespace Ui {
class Dialog_Trust_Demo;
}

class Dialog_Trust_Demo : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog_Trust_Demo(QWidget *parent = nullptr);
    ~Dialog_Trust_Demo();

private slots:
    void slot_ShowPicture();
    void on_pushButton_close_clicked();
    void on_pushButton_CI_select_clicked();
    void on_pushButton_execute_clicked();
    void on_pushButton_clicked();

private:
    bool bStop = false;
    int imgNum = 0;
    Ui::Dialog_Trust_Demo *ui;
    QTimer *qTimer;
    QStringList arg_secure_demo;
    QProcess *p_secure_demo;
};

#endif // DIALOG_TRUST_DEMO_H
