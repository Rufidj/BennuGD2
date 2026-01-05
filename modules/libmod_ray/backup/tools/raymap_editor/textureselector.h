#ifndef TEXTURESELECTOR_H
#define TEXTURESELECTOR_H

#include <QDialog>
#include <QMap>
#include <QPixmap>
#include <QScrollArea>
#include <QGridLayout>
#include <QPushButton>

class TextureSelector : public QDialog
{
    Q_OBJECT

public:
    explicit TextureSelector(const QMap<int, QPixmap> &textures, QWidget *parent = nullptr);
    
    int selectedTextureId() const { return m_selectedId; }
    
private slots:
    void onTextureClicked(int textureId);
    void onNoneClicked();
    
private:
    void setupUI();
    
    QMap<int, QPixmap> m_textures;
    int m_selectedId;
};

#endif // TEXTURESELECTOR_H
