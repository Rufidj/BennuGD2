#ifndef FPGEDITOR_H
#define FPGEDITOR_H

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QMap>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QTimer>
#include <QCloseEvent>
#include "mapdata.h"

class FPGEditor : public QDialog
{
    Q_OBJECT

public:
    explicit FPGEditor(QWidget *parent = nullptr);
    ~FPGEditor();

    void setFPGPath(const QString &path);
    void loadFPG();

signals:
    void fpgReloaded();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAddTexture();
    void onRemoveTexture();
    void onSaveFPG();
    void onSaveFPGAs();
    void onReloadFPG();
    void onTextureSelected(QListWidgetItem *item);
    void onBrowseImage();
    void onPlayAnimation();
    void onStopAnimation();
    void onFPSChanged(int fps);
    void onAnimationTick();
    void onSelectionChanged();

private:
    void setupUI();
    void updateTextureList();
    void updatePreview(int textureId);
    bool saveFPGFile(const QString &filepath);
    void setModified(bool modified);

    QString m_fpgPath;
    QVector<TextureEntry> m_textures;
    QMap<int, QPixmap> m_textureMap;
    int m_selectedTextureId;
    bool m_isModified;
    
    // Animation
    QTimer *m_animationTimer;
    QVector<int> m_animationFrames;
    int m_currentAnimFrame;
    bool m_isPlaying;

    // UI Components
    QListWidget *m_textureList;
    QLabel *m_previewLabel;
    QLabel *m_infoLabel;
    QSpinBox *m_textureIDSpinBox;
    QLineEdit *m_imagePathEdit;
    QPushButton *m_browseButton;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_saveButton;
    QPushButton *m_saveAsButton;
    QPushButton *m_reloadButton;
    QPushButton *m_closeButton;
    
    // Animation UI
    QGroupBox *m_animationGroup;
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QSpinBox *m_fpsSpinBox;
    QLabel *m_frameCountLabel;
    QLabel *m_currentFrameLabel;
};

#endif // FPGEDITOR_H
