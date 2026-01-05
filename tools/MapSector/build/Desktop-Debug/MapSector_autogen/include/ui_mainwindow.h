/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QSplitter *splitter;
    QGraphicsView *mapView;
    QWidget *rightPanel;
    QGroupBox *editModeGroup;
    QRadioButton *editModeRadio;
    QRadioButton *vertexModeRadio;
    QRadioButton *lineModeRadio;
    QRadioButton *sectorModeRadio;
    QRadioButton *flagModeRadio;
    QPushButton *addSectorButton;
    QPushButton *addWallButton;
    QGroupBox *heightGroup;
    QLabel *ceilingLabel;
    QSpinBox *ceilingHeightSpin;
    QLabel *floorLabel;
    QSpinBox *floorHeightSpin;
    QGroupBox *gridGroup;
    QCheckBox *gridCheck;
    QCheckBox *snapCheck;
    QGroupBox *textureGroup;
    QPushButton *addTextureButton;
    QLabel *wallTextureLabel;
    QPushButton *wallTextureThumb;
    QLabel *ceilingTextureLabel;
    QPushButton *ceilingTextureThumb;
    QLabel *floorTextureLabel;
    QPushButton *floorTextureThumb;
    QGroupBox *infoGroup;
    QLabel *coordXLabel;
    QLabel *coordXValue;
    QLabel *coordYLabel;
    QLabel *coordYValue;
    QLabel *texFileLabel;
    QLabel *elementLabel;
    QLabel *label;
    QListWidget *sectorList;
    QPushButton *editVerticesButton;
    QPushButton *deleteSectorButton;
    QPushButton *newMapButton;
    QPushButton *exportWLDButton;
    QPushButton *importWLDButton;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1685, 897);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        splitter = new QSplitter(centralwidget);
        splitter->setObjectName("splitter");
        splitter->setGeometry(QRect(0, 0, 2200, 761));
        splitter->setOrientation(Qt::Orientation::Horizontal);
        mapView = new QGraphicsView(splitter);
        mapView->setObjectName("mapView");
        splitter->addWidget(mapView);
        rightPanel = new QWidget(splitter);
        rightPanel->setObjectName("rightPanel");
        rightPanel->setMinimumSize(QSize(1200, 0));
        editModeGroup = new QGroupBox(rightPanel);
        editModeGroup->setObjectName("editModeGroup");
        editModeGroup->setGeometry(QRect(10, 10, 180, 140));
        editModeRadio = new QRadioButton(editModeGroup);
        editModeRadio->setObjectName("editModeRadio");
        editModeRadio->setGeometry(QRect(10, 20, 80, 20));
        editModeRadio->setChecked(true);
        vertexModeRadio = new QRadioButton(editModeGroup);
        vertexModeRadio->setObjectName("vertexModeRadio");
        vertexModeRadio->setGeometry(QRect(90, 20, 80, 20));
        lineModeRadio = new QRadioButton(editModeGroup);
        lineModeRadio->setObjectName("lineModeRadio");
        lineModeRadio->setGeometry(QRect(10, 45, 80, 20));
        sectorModeRadio = new QRadioButton(editModeGroup);
        sectorModeRadio->setObjectName("sectorModeRadio");
        sectorModeRadio->setGeometry(QRect(90, 45, 80, 20));
        flagModeRadio = new QRadioButton(editModeGroup);
        flagModeRadio->setObjectName("flagModeRadio");
        flagModeRadio->setGeometry(QRect(10, 70, 80, 20));
        addSectorButton = new QPushButton(editModeGroup);
        addSectorButton->setObjectName("addSectorButton");
        addSectorButton->setGeometry(QRect(10, 100, 80, 23));
        addWallButton = new QPushButton(editModeGroup);
        addWallButton->setObjectName("addWallButton");
        addWallButton->setGeometry(QRect(95, 100, 80, 23));
        heightGroup = new QGroupBox(rightPanel);
        heightGroup->setObjectName("heightGroup");
        heightGroup->setGeometry(QRect(10, 160, 180, 80));
        ceilingLabel = new QLabel(heightGroup);
        ceilingLabel->setObjectName("ceilingLabel");
        ceilingLabel->setGeometry(QRect(10, 20, 70, 20));
        ceilingHeightSpin = new QSpinBox(heightGroup);
        ceilingHeightSpin->setObjectName("ceilingHeightSpin");
        ceilingHeightSpin->setGeometry(QRect(85, 20, 80, 23));
        ceilingHeightSpin->setMinimum(-4095);
        ceilingHeightSpin->setMaximum(4095);
        floorLabel = new QLabel(heightGroup);
        floorLabel->setObjectName("floorLabel");
        floorLabel->setGeometry(QRect(10, 50, 70, 20));
        floorHeightSpin = new QSpinBox(heightGroup);
        floorHeightSpin->setObjectName("floorHeightSpin");
        floorHeightSpin->setGeometry(QRect(85, 50, 80, 23));
        floorHeightSpin->setMinimum(-4095);
        floorHeightSpin->setMaximum(4095);
        gridGroup = new QGroupBox(rightPanel);
        gridGroup->setObjectName("gridGroup");
        gridGroup->setGeometry(QRect(10, 250, 180, 60));
        gridCheck = new QCheckBox(gridGroup);
        gridCheck->setObjectName("gridCheck");
        gridCheck->setGeometry(QRect(10, 25, 70, 20));
        gridCheck->setChecked(true);
        snapCheck = new QCheckBox(gridGroup);
        snapCheck->setObjectName("snapCheck");
        snapCheck->setGeometry(QRect(90, 25, 70, 20));
        textureGroup = new QGroupBox(rightPanel);
        textureGroup->setObjectName("textureGroup");
        textureGroup->setGeometry(QRect(200, 10, 180, 220));
        addTextureButton = new QPushButton(textureGroup);
        addTextureButton->setObjectName("addTextureButton");
        addTextureButton->setGeometry(QRect(10, 20, 160, 23));
        wallTextureLabel = new QLabel(textureGroup);
        wallTextureLabel->setObjectName("wallTextureLabel");
        wallTextureLabel->setGeometry(QRect(10, 55, 60, 20));
        wallTextureThumb = new QPushButton(textureGroup);
        wallTextureThumb->setObjectName("wallTextureThumb");
        wallTextureThumb->setGeometry(QRect(70, 55, 48, 48));
        ceilingTextureLabel = new QLabel(textureGroup);
        ceilingTextureLabel->setObjectName("ceilingTextureLabel");
        ceilingTextureLabel->setGeometry(QRect(10, 110, 60, 20));
        ceilingTextureThumb = new QPushButton(textureGroup);
        ceilingTextureThumb->setObjectName("ceilingTextureThumb");
        ceilingTextureThumb->setGeometry(QRect(70, 110, 48, 48));
        floorTextureLabel = new QLabel(textureGroup);
        floorTextureLabel->setObjectName("floorTextureLabel");
        floorTextureLabel->setGeometry(QRect(10, 165, 60, 20));
        floorTextureThumb = new QPushButton(textureGroup);
        floorTextureThumb->setObjectName("floorTextureThumb");
        floorTextureThumb->setGeometry(QRect(70, 165, 48, 48));
        infoGroup = new QGroupBox(rightPanel);
        infoGroup->setObjectName("infoGroup");
        infoGroup->setGeometry(QRect(200, 240, 180, 120));
        coordXLabel = new QLabel(infoGroup);
        coordXLabel->setObjectName("coordXLabel");
        coordXLabel->setGeometry(QRect(10, 20, 30, 20));
        coordXValue = new QLabel(infoGroup);
        coordXValue->setObjectName("coordXValue");
        coordXValue->setGeometry(QRect(40, 20, 60, 20));
        coordYLabel = new QLabel(infoGroup);
        coordYLabel->setObjectName("coordYLabel");
        coordYLabel->setGeometry(QRect(10, 40, 30, 20));
        coordYValue = new QLabel(infoGroup);
        coordYValue->setObjectName("coordYValue");
        coordYValue->setGeometry(QRect(40, 40, 60, 20));
        texFileLabel = new QLabel(infoGroup);
        texFileLabel->setObjectName("texFileLabel");
        texFileLabel->setGeometry(QRect(10, 65, 160, 20));
        elementLabel = new QLabel(infoGroup);
        elementLabel->setObjectName("elementLabel");
        elementLabel->setGeometry(QRect(10, 85, 160, 20));
        label = new QLabel(rightPanel);
        label->setObjectName("label");
        label->setGeometry(QRect(10, 370, 71, 16));
        sectorList = new QListWidget(rightPanel);
        sectorList->setObjectName("sectorList");
        sectorList->setGeometry(QRect(10, 390, 371, 80));
        editVerticesButton = new QPushButton(rightPanel);
        editVerticesButton->setObjectName("editVerticesButton");
        editVerticesButton->setGeometry(QRect(10, 500, 110, 23));
        deleteSectorButton = new QPushButton(rightPanel);
        deleteSectorButton->setObjectName("deleteSectorButton");
        deleteSectorButton->setGeometry(QRect(130, 500, 110, 23));
        newMapButton = new QPushButton(rightPanel);
        newMapButton->setObjectName("newMapButton");
        newMapButton->setGeometry(QRect(10, 470, 110, 23));
        exportWLDButton = new QPushButton(rightPanel);
        exportWLDButton->setObjectName("exportWLDButton");
        exportWLDButton->setGeometry(QRect(130, 470, 110, 23));
        importWLDButton = new QPushButton(rightPanel);
        importWLDButton->setObjectName("importWLDButton");
        importWLDButton->setGeometry(QRect(250, 470, 110, 23));
        splitter->addWidget(rightPanel);
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1685, 20));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MapSector Editor - divmap3d style", nullptr));
        editModeGroup->setTitle(QCoreApplication::translate("MainWindow", "Modo de Edici\303\263n", nullptr));
        editModeRadio->setText(QCoreApplication::translate("MainWindow", "Edici\303\263n", nullptr));
        vertexModeRadio->setText(QCoreApplication::translate("MainWindow", "V\303\251rtice", nullptr));
        lineModeRadio->setText(QCoreApplication::translate("MainWindow", "L\303\255nea", nullptr));
        sectorModeRadio->setText(QCoreApplication::translate("MainWindow", "Sector", nullptr));
        flagModeRadio->setText(QCoreApplication::translate("MainWindow", "Bandera", nullptr));
        addSectorButton->setText(QCoreApplication::translate("MainWindow", "A\303\261adir sector", nullptr));
        addWallButton->setText(QCoreApplication::translate("MainWindow", "A\303\261adir pared", nullptr));
        heightGroup->setTitle(QCoreApplication::translate("MainWindow", "Alturas", nullptr));
        ceilingLabel->setText(QCoreApplication::translate("MainWindow", "Techo:", nullptr));
        floorLabel->setText(QCoreApplication::translate("MainWindow", "Suelo:", nullptr));
        gridGroup->setTitle(QCoreApplication::translate("MainWindow", "Grid", nullptr));
        gridCheck->setText(QCoreApplication::translate("MainWindow", "Grid", nullptr));
        snapCheck->setText(QCoreApplication::translate("MainWindow", "Snap", nullptr));
        textureGroup->setTitle(QCoreApplication::translate("MainWindow", "Texturas .fpg", nullptr));
        addTextureButton->setText(QCoreApplication::translate("MainWindow", "Cargar archivo .fpg", nullptr));
        wallTextureLabel->setText(QCoreApplication::translate("MainWindow", "Pared:", nullptr));
        ceilingTextureLabel->setText(QCoreApplication::translate("MainWindow", "Techo:", nullptr));
        floorTextureLabel->setText(QCoreApplication::translate("MainWindow", "Suelo:", nullptr));
        infoGroup->setTitle(QCoreApplication::translate("MainWindow", "Informaci\303\263n", nullptr));
        coordXLabel->setText(QCoreApplication::translate("MainWindow", "X:", nullptr));
        coordXValue->setText(QCoreApplication::translate("MainWindow", "0000", nullptr));
        coordYLabel->setText(QCoreApplication::translate("MainWindow", "Y:", nullptr));
        coordYValue->setText(QCoreApplication::translate("MainWindow", "0000", nullptr));
        texFileLabel->setText(QCoreApplication::translate("MainWindow", "Ning\303\272n archivo .fpg", nullptr));
        elementLabel->setText(QCoreApplication::translate("MainWindow", "Elemento: Ninguno", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "Sectores :", nullptr));
        editVerticesButton->setText(QCoreApplication::translate("MainWindow", "Editar vertices", nullptr));
        deleteSectorButton->setText(QCoreApplication::translate("MainWindow", "Eliminar sector", nullptr));
        newMapButton->setText(QCoreApplication::translate("MainWindow", "Nuevo mapa WLD", nullptr));
        exportWLDButton->setText(QCoreApplication::translate("MainWindow", "Exportar WLD", nullptr));
        importWLDButton->setText(QCoreApplication::translate("MainWindow", "Importar WLD", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
