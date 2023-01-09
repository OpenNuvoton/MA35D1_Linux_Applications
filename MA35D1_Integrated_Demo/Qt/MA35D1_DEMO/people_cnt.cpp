#include "people_cnt.h"
#include "mainwindow.h"
#include <QObject>

#define PERIOD_TIME (0.25*1000)

People_Cnt::People_Cnt(){
    b_stop = false;
}

People_Cnt::~People_Cnt(){
}

void People_Cnt::run(){
    qDebug()<<"Counter thread is :"<<QThread::currentThreadId()<<endl;
    while(!b_stop)
    {
        if(pWnd_cam->GetFaceDetectSts()){
            pWnd_cam->face_detect_alg();
        }

        QThread::currentThread()->msleep(PERIOD_TIME);
    }
    b_stop = false;
}


void People_Cnt::stop()
{
    b_stop = true;
}
