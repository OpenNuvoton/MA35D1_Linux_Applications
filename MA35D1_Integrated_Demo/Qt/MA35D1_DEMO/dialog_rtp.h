#ifndef DIALOG_RTP_H
#define DIALOG_RTP_H

#include <QDialog>
#include <QKeyEvent>
#include <QtDebug>
#include <vector>
#include <iostream>

namespace Ui {
class Dialog_RTP;
}

class Dialog_RTP : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog_RTP(QWidget *parent = nullptr);
    void keyPressEvent(QKeyEvent *event);
    int prev_num = 1;
    void BtnSts(int n, bool sts);
    ~Dialog_RTP();

    std::vector<QPushButton*> nums;


private slots:
    void on_pushButton_close_clicked();
    void on_pushButton_RTP_start_clicked();

private:
    Ui::Dialog_RTP *ui;
    bool bStop = true;
};

#endif // DIALOG_RTP_H
