/****************************************************************************
** Meta object code from reading C++ file 'EditorScene.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../EditorScene.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'EditorScene.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11EditorSceneE_t {};
} // unnamed namespace

template <> constexpr inline auto EditorScene::qt_create_metaobjectdata<qt_meta_tag_ZN11EditorSceneE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "EditorScene",
        "vertexAdded",
        "",
        "pos",
        "polygonFinished",
        "wallPointAdded",
        "wallFinished",
        "sectorClicked",
        "sectorIndex",
        "mouseMoved"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'vertexAdded'
        QtMocHelpers::SignalData<void(QPointF)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QPointF, 3 },
        }}),
        // Signal 'polygonFinished'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'wallPointAdded'
        QtMocHelpers::SignalData<void(QPointF)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QPointF, 3 },
        }}),
        // Signal 'wallFinished'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sectorClicked'
        QtMocHelpers::SignalData<void(int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 },
        }}),
        // Signal 'mouseMoved'
        QtMocHelpers::SignalData<void(QPointF)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QPointF, 3 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<EditorScene, qt_meta_tag_ZN11EditorSceneE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject EditorScene::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsScene::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11EditorSceneE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11EditorSceneE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11EditorSceneE_t>.metaTypes,
    nullptr
} };

void EditorScene::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<EditorScene *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->vertexAdded((*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[1]))); break;
        case 1: _t->polygonFinished(); break;
        case 2: _t->wallPointAdded((*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[1]))); break;
        case 3: _t->wallFinished(); break;
        case 4: _t->sectorClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->mouseMoved((*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (EditorScene::*)(QPointF )>(_a, &EditorScene::vertexAdded, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditorScene::*)()>(_a, &EditorScene::polygonFinished, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditorScene::*)(QPointF )>(_a, &EditorScene::wallPointAdded, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditorScene::*)()>(_a, &EditorScene::wallFinished, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditorScene::*)(int )>(_a, &EditorScene::sectorClicked, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (EditorScene::*)(QPointF )>(_a, &EditorScene::mouseMoved, 5))
            return;
    }
}

const QMetaObject *EditorScene::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *EditorScene::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11EditorSceneE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsScene::qt_metacast(_clname);
}

int EditorScene::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsScene::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void EditorScene::vertexAdded(QPointF _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void EditorScene::polygonFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void EditorScene::wallPointAdded(QPointF _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void EditorScene::wallFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void EditorScene::sectorClicked(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void EditorScene::mouseMoved(QPointF _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
