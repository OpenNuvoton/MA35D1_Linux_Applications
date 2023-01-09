QT       += core gui multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    dialog_cam.cpp \
    dialog_rtp.cpp \
    dialog_trust_demo.cpp \
    main.cpp \
    mainwindow.cpp \
    mtcnn/detector.cpp \
    mtcnn/onet.cpp \
    mtcnn/pnet.cpp \
    mtcnn/rnet.cpp \
    people_cnt.cpp \
    sharedviewfinder.cpp

HEADERS += \
    dialog_cam.h \
    dialog_rtp.h \
    dialog_trust_demo.h \
    draw.hpp \
    mainwindow.h \
    mtcnn/detector.h \
    mtcnn/face.h \
    mtcnn/helpers.h \
    mtcnn/onet.h \
    mtcnn/pnet.h \
    mtcnn/rnet.h \
    people_cnt.h \
    sharedviewfinder.h

FORMS += \
    dialog_2d_engine.ui \
    dialog_cam.ui \
    dialog_rtp.ui \
    dialog_trust_demo.ui \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \



# FOR X86 BASE
#INCLUDEPATH += /usr/local/include/opencv4
#LIBS += -L/usr/local/lib -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_features2d -lopencv_imgproc -lopencv_objdetect -lopencv_dnn

#FOR ARM BASE
INCLUDEPATH += ./include/opencv
LIBS += -L ./lib -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_features2d -lopencv_imgproc -lopencv_objdetect -lopencv_dnn

RESOURCES += \
    Resouces.qrc
