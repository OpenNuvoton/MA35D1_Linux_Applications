/****************************************************************************
** Meta object code from reading C++ file 'dialog_cam.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../MA35D1_DEMO/dialog_cam.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dialog_cam.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_Dialog_Cam_t {
    QByteArrayData data[12];
    char stringdata0[209];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Dialog_Cam_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Dialog_Cam_t qt_meta_stringdata_Dialog_Cam = {
    {
QT_MOC_LITERAL(0, 0, 10), // "Dialog_Cam"
QT_MOC_LITERAL(1, 11, 27), // "on_pushButton_close_clicked"
QT_MOC_LITERAL(2, 39, 0), // ""
QT_MOC_LITERAL(3, 40, 32), // "on_comboBox_cam_select_activated"
QT_MOC_LITERAL(4, 73, 5), // "index"
QT_MOC_LITERAL(5, 79, 32), // "on_comboBox_resolution_activated"
QT_MOC_LITERAL(6, 112, 13), // "setfblComobox"
QT_MOC_LITERAL(7, 126, 33), // "on_radioButton_people_cnt_cli..."
QT_MOC_LITERAL(8, 160, 7), // "checked"
QT_MOC_LITERAL(9, 168, 10), // "frameReady"
QT_MOC_LITERAL(10, 179, 6), // "pixmap"
QT_MOC_LITERAL(11, 186, 22) // "slot_People_Cnt_Enable"

    },
    "Dialog_Cam\0on_pushButton_close_clicked\0"
    "\0on_comboBox_cam_select_activated\0"
    "index\0on_comboBox_resolution_activated\0"
    "setfblComobox\0on_radioButton_people_cnt_clicked\0"
    "checked\0frameReady\0pixmap\0"
    "slot_People_Cnt_Enable"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Dialog_Cam[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x08 /* Private */,
       3,    1,   50,    2, 0x08 /* Private */,
       5,    1,   53,    2, 0x08 /* Private */,
       6,    0,   56,    2, 0x08 /* Private */,
       7,    1,   57,    2, 0x08 /* Private */,
       9,    1,   60,    2, 0x08 /* Private */,
      11,    0,   63,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    8,
    QMetaType::Void, QMetaType::QPixmap,   10,
    QMetaType::Int,

       0        // eod
};

void Dialog_Cam::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Dialog_Cam *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->on_pushButton_close_clicked(); break;
        case 1: _t->on_comboBox_cam_select_activated((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->on_comboBox_resolution_activated((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->setfblComobox(); break;
        case 4: _t->on_radioButton_people_cnt_clicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: _t->frameReady((*reinterpret_cast< QPixmap(*)>(_a[1]))); break;
        case 6: { int _r = _t->slot_People_Cnt_Enable();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject Dialog_Cam::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_Dialog_Cam.data,
    qt_meta_data_Dialog_Cam,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *Dialog_Cam::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Dialog_Cam::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Dialog_Cam.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int Dialog_Cam::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
