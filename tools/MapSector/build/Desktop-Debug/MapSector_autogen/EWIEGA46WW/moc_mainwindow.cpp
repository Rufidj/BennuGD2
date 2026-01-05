/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../mainwindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "on_addSectorButton_clicked",
        "",
        "on_addWallButton_clicked",
        "on_addTextureButton_clicked",
        "on_exportWLDButton_clicked",
        "on_importWLDButton_clicked",
        "on_newMapButton_clicked",
        "on_sectorList_currentRowChanged",
        "index",
        "on_floorHeightSpin_valueChanged",
        "value",
        "on_ceilingHeightSpin_valueChanged",
        "onVertexAdded",
        "pos",
        "onPolygonFinished",
        "on_editVerticesButton_clicked",
        "on_deleteSectorButton_clicked",
        "onSectorMoved",
        "sectorIndex",
        "delta",
        "onVertexMoved",
        "vertexIndex",
        "newPosition",
        "onWallPointAdded",
        "onWallFinished",
        "on_wallTextureThumb_clicked",
        "on_ceilingTextureThumb_clicked",
        "on_floorTextureThumb_clicked",
        "onSectorClicked",
        "redrawVerticesOnly"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'on_addSectorButton_clicked'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_addWallButton_clicked'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_addTextureButton_clicked'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_exportWLDButton_clicked'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_importWLDButton_clicked'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_newMapButton_clicked'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_sectorList_currentRowChanged'
        QtMocHelpers::SlotData<void(int)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'on_floorHeightSpin_valueChanged'
        QtMocHelpers::SlotData<void(double)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Double, 11 },
        }}),
        // Slot 'on_ceilingHeightSpin_valueChanged'
        QtMocHelpers::SlotData<void(double)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Double, 11 },
        }}),
        // Slot 'onVertexAdded'
        QtMocHelpers::SlotData<void(QPointF)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPointF, 14 },
        }}),
        // Slot 'onPolygonFinished'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_editVerticesButton_clicked'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_deleteSectorButton_clicked'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSectorMoved'
        QtMocHelpers::SlotData<void(int, QPointF)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::QPointF, 20 },
        }}),
        // Slot 'onVertexMoved'
        QtMocHelpers::SlotData<void(int, int, QPointF)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 22 }, { QMetaType::QPointF, 23 },
        }}),
        // Slot 'onWallPointAdded'
        QtMocHelpers::SlotData<void(QPointF)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPointF, 14 },
        }}),
        // Slot 'onWallFinished'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_wallTextureThumb_clicked'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_ceilingTextureThumb_clicked'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_floorTextureThumb_clicked'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSectorClicked'
        QtMocHelpers::SlotData<void(int)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 19 },
        }}),
        // Slot 'redrawVerticesOnly'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->on_addSectorButton_clicked(); break;
        case 1: _t->on_addWallButton_clicked(); break;
        case 2: _t->on_addTextureButton_clicked(); break;
        case 3: _t->on_exportWLDButton_clicked(); break;
        case 4: _t->on_importWLDButton_clicked(); break;
        case 5: _t->on_newMapButton_clicked(); break;
        case 6: _t->on_sectorList_currentRowChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->on_floorHeightSpin_valueChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 8: _t->on_ceilingHeightSpin_valueChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 9: _t->onVertexAdded((*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[1]))); break;
        case 10: _t->onPolygonFinished(); break;
        case 11: _t->on_editVerticesButton_clicked(); break;
        case 12: _t->on_deleteSectorButton_clicked(); break;
        case 13: _t->onSectorMoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 14: _t->onVertexMoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[3]))); break;
        case 15: _t->onWallPointAdded((*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[1]))); break;
        case 16: _t->onWallFinished(); break;
        case 17: _t->on_wallTextureThumb_clicked(); break;
        case 18: _t->on_ceilingTextureThumb_clicked(); break;
        case 19: _t->on_floorTextureThumb_clicked(); break;
        case 20: _t->onSectorClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 21: _t->redrawVerticesOnly(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 22)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 22;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 22)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 22;
    }
    return _id;
}
QT_WARNING_POP
