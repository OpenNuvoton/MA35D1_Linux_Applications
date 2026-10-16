QT += core gui widgets xml network
CONFIG += c++17
DEFINES += QT_NO_KEYWORDS
TARGET = smart-home-overlay
TEMPLATE = app
SOURCES += src/main.cpp src/model.cpp
HEADERS += src/model.hpp src/autotune.hpp src/adaptive.hpp src/adaptive_ui.inc src/slideshow.hpp
isEmpty(LVGL_SOURCE) { LVGL_SOURCE=$$PWD/vendor/lvgl }
INCLUDEPATH += . $$LVGL_SOURCE ../native-media
DEFINES += LV_CONF_PATH=\\\"$$PWD/lv_conf.h\\\"
isEmpty(LVGL_ARCH) { LVGL_ARCH=host }
LIBS += $$PWD/build/$$LVGL_ARCH/liblvgl.a -lm -lpthread
