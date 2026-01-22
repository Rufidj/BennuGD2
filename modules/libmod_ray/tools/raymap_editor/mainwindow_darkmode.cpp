#include "mainwindow.h"
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QSettings>
#include "projectmanager.h"
#include "assetbrowser.h"
#include <QFileInfo>

void MainWindow::onToggleDarkMode(bool checked)
{
    if (checked) {
        qApp->setStyle(QStyleFactory::create("Fusion"));

        QPalette p;
        p.setColor(QPalette::Window, QColor(53, 53, 53));
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, QColor(42, 42, 42)); // Text inputs
        p.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
        p.setColor(QPalette::ToolTipBase, Qt::white);
        p.setColor(QPalette::ToolTipText, Qt::white);
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, QColor(53, 53, 53));
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(42, 130, 218));
        p.setColor(QPalette::Highlight, QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::black);
        
        // Disabled colors
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(128, 128, 128));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));

        qApp->setPalette(p);
    } else {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(style()->standardPalette());
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("BennuGD", "RayMapEditor");
    
    // Window geometry
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // Dark Mode
    bool darkMode = settings.value("darkMode", true).toBool(); // Default to Dark Mode
    onToggleDarkMode(darkMode);
    
    // Recent files
    updateRecentMapsMenu();
    updateRecentFPGsMenu();
    
    
    // Last Open Project - DISABLED: User requested not to auto-load
    /*
    QString lastProject = settings.value("lastOpenProject").toString();
    if (!lastProject.isEmpty() && QFile::exists(lastProject)) {
        if (m_projectManager) {
            if (m_projectManager->openProject(lastProject)) {
                if (m_assetBrowser) {
                    m_assetBrowser->setProjectPath(m_projectManager->getProjectPath());
                }
                updateWindowTitle();
                 
                 // Also open main.prg if it exists?
                 // Maybe later.
            }
        }
    }
    */
}

void MainWindow::saveSettings()
{
    QSettings settings("BennuGD", "RayMapEditor");
    
    // Window geometry
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    // Dark Mode
    // We check the palette to know if we are in dark mode, or store a member variable.
    // Simpler: Just check if Window color is dark.
    bool specificDarkMode = (qApp->palette().color(QPalette::Window).lightness() < 128);
    settings.setValue("darkMode", specificDarkMode);
    
    // Last Open Project
    if (m_projectManager && m_projectManager->hasProject()) {
        settings.setValue("lastOpenProject", m_projectManager->getProjectPath() + "/" + m_projectManager->getProject()->name + ".bgd2proj");
    } else {
        settings.remove("lastOpenProject");
    }
}
