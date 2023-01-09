#ifndef DIALOG_CAM_H
#define DIALOG_CAM_H

#include <QDialog>
#include <QCameraInfo>
#include <QCameraViewfinder>
#include <QComboBox>
#include <QVideoSurfaceFormat>
#include <QThread>
#include <QTimer>
#include <QTime>
#include <QCameraImageCapture>
#include <QPainter>
#include "sharedviewfinder.h"
#include "people_cnt.h"
#include "draw.hpp"

namespace Ui {
class Dialog_Cam;
}

class Dialog_Cam : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog_Cam(QWidget *parent = nullptr);
    ~Dialog_Cam();

    bool GetFaceDetectSts();
    int face_detect_alg();

private slots:
    void on_pushButton_close_clicked();
    void on_pushButton_people_cnt_clicked();
    void on_comboBox_cam_select_activated(int index);
    void on_comboBox_resolution_activated(int index);

    void setfblComobox();
    void frameReady(QPixmap pixmap);

private:
    Ui::Dialog_Cam *ui;
    bool b_facedetect_done;
    bool b_peoplecnt_en;
    int find_resolution_idx(int resolution);
    int idx_resolution_display;
    int idx_resolution_facedetect;

    float mappingfactor_width;
    float mappingfactor_height;

    QCamera *camera;
    SharedViewfinder  *viewfinder;
    QList<QCameraInfo> cameras;
    QList<QSize> mResSize = {};
    QComboBox *box;
    QPixmap frame_capture;
    QTimer *timer_people_cnt;
    QPixmap frame_pixmap;
    std::vector<rectPoints> data;
    People_Cnt  *thread_people_cnt;

    void printResolutionSize();
    int init_Cam();
    int open_Cam();
    int close_Cam();

signals:
};

#endif // DIALOG_CAM_H
