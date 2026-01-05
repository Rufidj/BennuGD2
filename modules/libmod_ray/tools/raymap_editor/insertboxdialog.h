#ifndef INSERTBOXDIALOG_H
#define INSERTBOXDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QPixmap>
#include <QMap>

class InsertBoxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InsertBoxDialog(const QMap<int, QPixmap> &textureCache, QWidget *parent = nullptr);
    
    // Getters for configuration
    float getWidth() const { return m_widthSpin->value(); }
    float getHeight() const { return m_heightSpin->value(); }
    float getFloorZ() const { return m_floorZSpin->value(); }
    float getCeilingZ() const { return m_ceilingZSpin->value(); }
    int getWallTexture() const { return m_wallTextureCombo->currentData().toInt(); }
    int getFloorTexture() const { return m_floorTextureCombo->currentData().toInt(); }
    int getCeilingTexture() const { return m_ceilingTextureCombo->currentData().toInt(); }

private:
    QDoubleSpinBox *m_widthSpin;
    QDoubleSpinBox *m_heightSpin;
    QDoubleSpinBox *m_floorZSpin;
    QDoubleSpinBox *m_ceilingZSpin;
    QComboBox *m_wallTextureCombo;
    QComboBox *m_floorTextureCombo;
    QComboBox *m_ceilingTextureCombo;
    QDialogButtonBox *m_buttonBox;
};

#endif // INSERTBOXDIALOG_H
