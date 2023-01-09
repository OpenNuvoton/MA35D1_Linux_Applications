#include "mainwindow.h"
#include <iostream>
#include <vector>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
//    w.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    w.show();
    pWnd_main = &w;
    qDebug()<<"main thread is :"<<QThread::currentThreadId()<<endl;

    return a.exec();
}
