#ifndef PEOPLE_CNT_H
#define PEOPLE_CNT_H


#include <QThread>
#include <QDebug>
//#include "dialog_cam.h"

class People_Cnt : public QThread {

    Q_OBJECT
public:
    People_Cnt();
    ~People_Cnt();
    void run();
    void stop();
    QString name;

signals:
    void signal_frame_to_pep_cnt();

private:
    volatile bool b_stop;
};

#endif // PEOPLE_CNT_H
