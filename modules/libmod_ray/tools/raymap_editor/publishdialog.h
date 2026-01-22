#ifndef PUBLISHDIALOG_H
#define PUBLISHDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QProgressBar>
#include <QStandardPaths>
#include "projectmanager.h"
#include "publisher.h"
#include "downloader.h"

class PublishDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PublishDialog(ProjectData *project, QWidget *parent = nullptr);

private slots:
    void onBrowseOutput();
    void onBrowseIcon();
    void onPublish();
    void onPlatformChanged(int index);
    void onDownloadAppImageTool();
    void onDownloadNDK(); // NEW // NEW
    void refreshWindowsTools(); // Refresh detection of Windows tools

private:
    QString findToolPath(const QString &toolName); // Helper to find tools aggressively
    ProjectData *m_project;
    Publisher m_publisher;
    
    // Paths
    QString m_appImageToolPath;
    QString m_ndkPath;
    
    // UI Elements
    QComboBox *m_platformCombo;
    QLineEdit *m_outputPathEdit;
    
    // Linux Options
    QWidget *m_linuxOptions;
    QCheckBox *m_chkLinuxArchive;
    QCheckBox *m_chkLinuxAppImage;

    // Windows Options
    QWidget *m_windowsOptions;
    QCheckBox *m_chkWindowsStandalone;
    QCheckBox *m_chkWindowsSfx;
    QCheckBox *m_chkWindowsZip;
    QPushButton *m_installMingwBtn;
    QPushButton *m_install7zBtn;

    // Android Options
    QWidget *m_androidOptions;
    QLineEdit *m_packageNameEdit;
    QLineEdit *m_iconPathEdit;
    QCheckBox *m_chkAndroidProject;
    QCheckBox *m_chkAndroidAPK;

    QProgressBar *m_progressBar;
    QPushButton *m_publishButton;
    QPushButton *m_closeButton; // Rename to Close or Cancel

    void setupUI();
};

#endif // PUBLISHDIALOG_H
