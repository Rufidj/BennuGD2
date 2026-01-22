#ifndef PROJECTSETTINGSDIALOG_H
#define PROJECTSETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include "projectmanager.h"

class ProjectSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectSettingsDialog(const ProjectData &data, QWidget *parent = nullptr);
    
    // Returns the modified project data
    ProjectData getProjectData() const;

private slots:
    void onBrowseFPG();
    void onBrowseMap();
    void onAccept();

private:
    void setupUi();
    
    ProjectData m_data; // Working copy

    // UI Elements
    QLineEdit *m_nameEdit;
    QLineEdit *m_versionEdit;
    
    QLineEdit *m_fpgEdit;
    QPushButton *m_browseFpgBtn;
    
    QLineEdit *m_mapEdit;
    QPushButton *m_browseMapBtn;
    
    QSpinBox *m_widthSpin;
    QSpinBox *m_heightSpin;
    QSpinBox *m_renderWidthSpin;  // New: Internal render resolution
    QSpinBox *m_renderHeightSpin; // New: Internal render resolution
    QSpinBox *m_fpsSpin;
    QSpinBox *m_fovSpin;
    QSpinBox *m_qualitySpin; // New
    QCheckBox *m_fullscreenCheck; // New: Fullscreen mode
    
    QDoubleSpinBox *m_camX;
    QDoubleSpinBox *m_camY;
    QDoubleSpinBox *m_camZ;
    QDoubleSpinBox *m_camRot;
    QDoubleSpinBox *m_camPitch;
};

#endif // PROJECTSETTINGSDIALOG_H
