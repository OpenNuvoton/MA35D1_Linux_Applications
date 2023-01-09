#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QProcess>
#include <QMouseEvent>
#include "dialog_cam.h"
#include "dialog_2d_engine.h"
#include "dialog_trust_demo.h"
#include "dialog_rtp.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    int Init();
    bool b_2d_ghc_on;
    bool b_dlg_cam_on;
    bool b_dlg_trust_demo_on;

private slots:
    void mousePressEvent(QMouseEvent *event);

    void on_pushButton_h264dec_clicked();
    void on_pushButton_h264dec_released();
    void on_pushButton_h264dec_pressed();

    void on_pushButton_peoplecnt_clicked();
    void on_pushButton_peoplecnt_pressed();
    void on_pushButton_peoplecnt_released();

    void on_pushButton_2d_ghc_clicked();
    void on_pushButton_2d_ghc_pressed();
    void on_pushButton_2d_ghc_released();

    void on_pushButton_trust_demo_pressed();
    void on_pushButton_trust_demo_released();
    void on_pushButton_trust_demo_clicked();

    void on_pushButton_rtp_clicked();
    void on_pushButton_rtp_pressed();
    void on_pushButton_rtp_released();

    void on_pushButton_sip_phone_clicked();
    void on_pushButton_sip_phone_pressed();
    void on_pushButton_sip_phone_released();

    void slot_video_play_stop();
    void slot_baresip_stop();

private:
    void pushButton_stsSet(bool);
    Ui::MainWindow *ui;
    Dialog_Cam *dlg_cam;
    Dialog_Trust_Demo *dlg_trust_demo;
    Dialog_RTP *dlg_rtp;
    QProcess *p_video_play;
    QProcess *p_phone_gui;
};

extern   Dialog_Cam  *pWnd_cam;
extern   MainWindow  *pWnd_main;

#endif // MAINWINDOW_H

