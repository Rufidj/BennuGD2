#include "fpgeditor.h"
#include "fpgloader.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QFormLayout>
#include <QFile>
#include <QDataStream>
#include <QBuffer>
#include <QImageReader>
#include <QLineEdit>

FPGEditor::FPGEditor(QWidget *parent)
    : QDialog(parent)
    , m_selectedTextureId(-1)
    , m_isModified(false)
    , m_animationTimer(new QTimer(this))
    , m_currentAnimFrame(0)
    , m_isPlaying(false)
{
    setupUI();
    setWindowTitle(tr("Editor de FPG"));
    resize(900, 600);
    
    // Connect animation timer
    connect(m_animationTimer, &QTimer::timeout, this, &FPGEditor::onAnimationTick);
}

FPGEditor::~FPGEditor()
{
}

void FPGEditor::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    // Left panel: Texture list
    QVBoxLayout *leftLayout = new QVBoxLayout();
    
    QLabel *listLabel = new QLabel(tr("Texturas:"));
    leftLayout->addWidget(listLabel);
    
    m_textureList = new QListWidget();
    m_textureList->setIconSize(QSize(64, 64));
    m_textureList->setViewMode(QListWidget::ListMode);
    m_textureList->setSelectionMode(QAbstractItemView::ExtendedSelection);  // Multi-selection
    connect(m_textureList, &QListWidget::itemClicked, this, &FPGEditor::onTextureSelected);
    connect(m_textureList, &QListWidget::itemSelectionChanged, this, &FPGEditor::onSelectionChanged);
    leftLayout->addWidget(m_textureList);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *newButton = new QPushButton(tr("Nuevo FPG"));
    connect(newButton, &QPushButton::clicked, this, [this]() {
        // Check for unsaved changes
        if (m_isModified) {
            QMessageBox::StandardButton reply = QMessageBox::question(this,
                tr("Cambios sin Guardar"),
                tr("Hay cambios sin guardar. ¿Continuar de todos modos?"),
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::No) {
                return;
            }
        }
        
        // Clear everything
        m_textures.clear();
        m_textureMap.clear();
        m_fpgPath.clear();
        m_selectedTextureId = -1;
        
        updateTextureList();
        m_previewLabel->clear();
        m_previewLabel->setText(tr("Ninguna textura seleccionada"));
        m_infoLabel->clear();
        
        // Reset ID spinner to 1
        m_textureIDSpinBox->setValue(1);
        
        // Enable save as button (for new FPG)
        m_saveAsButton->setEnabled(true);
        m_saveButton->setEnabled(false);
        m_reloadButton->setEnabled(false);
        
        setModified(false);
        setWindowTitle(tr("Editor de FPG - Nuevo"));
        
        QMessageBox::information(this, tr("Nuevo FPG"),
                               tr("FPG vacío creado. Añade texturas y usa 'Guardar Como...' para guardarlo."));
    });
    buttonLayout->addWidget(newButton);
    
    QPushButton *loadButton = new QPushButton(tr("Cargar FPG..."));
    connect(loadButton, &QPushButton::clicked, this, [this]() {
        QString filename = QFileDialog::getOpenFileName(this, 
            tr("Cargar FPG"),
            "",
            tr("Archivos FPG (*.fpg)"));
        
        if (!filename.isEmpty()) {
            setFPGPath(filename);
            loadFPG();
        }
    });
    buttonLayout->addWidget(loadButton);
    
    m_removeButton = new QPushButton(tr("Eliminar"));
    m_removeButton->setEnabled(false);
    connect(m_removeButton, &QPushButton::clicked, this, &FPGEditor::onRemoveTexture);
    buttonLayout->addWidget(m_removeButton);
    
    leftLayout->addLayout(buttonLayout);
    
    // Save buttons row
    QHBoxLayout *saveButtonLayout = new QHBoxLayout();
    
    m_saveButton = new QPushButton(tr("Guardar"));
    m_saveButton->setEnabled(false);
    connect(m_saveButton, &QPushButton::clicked, this, &FPGEditor::onSaveFPG);
    saveButtonLayout->addWidget(m_saveButton);
    
    m_saveAsButton = new QPushButton(tr("Guardar Como..."));
    m_saveAsButton->setEnabled(false);
    connect(m_saveAsButton, &QPushButton::clicked, this, &FPGEditor::onSaveFPGAs);
    saveButtonLayout->addWidget(m_saveAsButton);
    
    m_reloadButton = new QPushButton(tr("Recargar"));
    m_reloadButton->setEnabled(false);
    connect(m_reloadButton, &QPushButton::clicked, this, &FPGEditor::onReloadFPG);
    saveButtonLayout->addWidget(m_reloadButton);
    
    leftLayout->addLayout(saveButtonLayout);
    
    mainLayout->addLayout(leftLayout, 1);

    // Right panel: Preview and add texture
    QVBoxLayout *rightLayout = new QVBoxLayout();
    
    // Preview group
    QGroupBox *previewGroup = new QGroupBox(tr("Vista Previa"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumSize(400, 300);
    
    m_previewLabel = new QLabel();
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setText(tr("Ninguna textura seleccionada"));
    m_previewLabel->setStyleSheet("QLabel { background-color: #2b2b2b; color: #888; }");
    scrollArea->setWidget(m_previewLabel);
    
    previewLayout->addWidget(scrollArea);
    
    m_infoLabel = new QLabel();
    m_infoLabel->setWordWrap(true);
    previewLayout->addWidget(m_infoLabel);
    
    rightLayout->addWidget(previewGroup);
    
    // Animation group
    m_animationGroup = new QGroupBox(tr("Animación"));
    QVBoxLayout *animLayout = new QVBoxLayout(m_animationGroup);
    
    m_frameCountLabel = new QLabel(tr("Frames seleccionados: 0"));
    animLayout->addWidget(m_frameCountLabel);
    
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    
    m_playButton = new QPushButton(tr("▶ Reproducir"));
    m_playButton->setEnabled(false);
    connect(m_playButton, &QPushButton::clicked, this, &FPGEditor::onPlayAnimation);
    controlsLayout->addWidget(m_playButton);
    
    m_stopButton = new QPushButton(tr("⏹ Detener"));
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &FPGEditor::onStopAnimation);
    controlsLayout->addWidget(m_stopButton);
    
    animLayout->addLayout(controlsLayout);
    
    QHBoxLayout *fpsLayout = new QHBoxLayout();
    QLabel *fpsLabel = new QLabel(tr("FPS:"));
    fpsLayout->addWidget(fpsLabel);
    
    m_fpsSpinBox = new QSpinBox();
    m_fpsSpinBox->setRange(1, 120);
    m_fpsSpinBox->setValue(12);
    connect(m_fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &FPGEditor::onFPSChanged);
    fpsLayout->addWidget(m_fpsSpinBox);
    fpsLayout->addStretch();
    
    animLayout->addLayout(fpsLayout);
    
    m_currentFrameLabel = new QLabel(tr("Frame: -/-"));
    animLayout->addWidget(m_currentFrameLabel);
    
    rightLayout->addWidget(m_animationGroup);
    
    // Add texture group
    QGroupBox *addGroup = new QGroupBox(tr("Añadir Nueva Textura"));
    QFormLayout *addLayout = new QFormLayout(addGroup);
    
    m_textureIDSpinBox = new QSpinBox();
    m_textureIDSpinBox->setRange(1, 9999);
    m_textureIDSpinBox->setValue(1);
    addLayout->addRow(tr("ID de Textura:"), m_textureIDSpinBox);
    
    QHBoxLayout *imageLayout = new QHBoxLayout();
    m_imagePathEdit = new QLineEdit();
    m_imagePathEdit->setPlaceholderText(tr("Selecciona un archivo de imagen..."));
    imageLayout->addWidget(m_imagePathEdit);
    
    m_browseButton = new QPushButton(tr("Examinar..."));
    connect(m_browseButton, &QPushButton::clicked, this, &FPGEditor::onBrowseImage);
    imageLayout->addWidget(m_browseButton);
    
    addLayout->addRow(tr("Imagen:"), imageLayout);
    
    m_addButton = new QPushButton(tr("Añadir al FPG"));
    m_addButton->setEnabled(false);
    connect(m_addButton, &QPushButton::clicked, this, &FPGEditor::onAddTexture);
    addLayout->addRow(m_addButton);
    
    rightLayout->addWidget(addGroup);
    
    // Close button
    m_closeButton = new QPushButton(tr("Cerrar"));
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    rightLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(rightLayout, 1);
    
    // Enable add button when image path is set
    connect(m_imagePathEdit, &QLineEdit::textChanged, [this](const QString &text) {
        m_addButton->setEnabled(!text.isEmpty());
    });
}

void FPGEditor::setFPGPath(const QString &path)
{
    m_fpgPath = path;
    setWindowTitle(tr("FPG Editor - %1").arg(QFileInfo(path).fileName()));
}

void FPGEditor::loadFPG()
{
    if (m_fpgPath.isEmpty()) return;
    
    m_textures.clear();
    m_textureMap.clear();
    
    bool success = FPGLoader::loadFPG(m_fpgPath, m_textures);
    
    if (success) {
        m_textureMap = FPGLoader::getTextureMap(m_textures);
        updateTextureList();
        
        // Find next available ID
        int maxId = 0;
        for (const auto &tex : m_textures) {
            if (tex.id > maxId) maxId = tex.id;
        }
        m_textureIDSpinBox->setValue(maxId + 1);
        
        // Enable save/reload buttons
        m_saveButton->setEnabled(true);
        m_saveAsButton->setEnabled(true);
        m_reloadButton->setEnabled(true);
        
        // Clear modified flag after loading
        setModified(false);
    } else {
        QMessageBox::warning(this, tr("Error"), 
                           tr("Error al cargar el archivo FPG: %1").arg(m_fpgPath));
    }
}

void FPGEditor::updateTextureList()
{
    m_textureList->clear();
    
    for (const auto &tex : m_textures) {
        QListWidgetItem *item = new QListWidgetItem();
        
        int width = tex.pixmap.width();
        int height = tex.pixmap.height();
        item->setText(tr("[%1] %2x%3").arg(tex.id).arg(width).arg(height));
        
        if (m_textureMap.contains(tex.id)) {
            QPixmap pixmap = m_textureMap[tex.id];
            QPixmap thumbnail = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            item->setIcon(QIcon(thumbnail));
        }
        
        item->setData(Qt::UserRole, tex.id);
        m_textureList->addItem(item);
    }
}

void FPGEditor::updatePreview(int textureId)
{
    if (!m_textureMap.contains(textureId)) {
        m_previewLabel->setText(tr("Textura no encontrada"));
        m_infoLabel->clear();
        return;
    }
    
    QPixmap pixmap = m_textureMap[textureId];
    
    // Scale preview if too large
    QPixmap preview = pixmap;
    if (pixmap.width() > 400 || pixmap.height() > 400) {
        preview = pixmap.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    m_previewLabel->setPixmap(preview);
    
    // Find texture info
    for (const auto &tex : m_textures) {
        if (tex.id == textureId) {
            m_infoLabel->setText(tr("ID: %1\nSize: %2 x %3\nFile: %4")
                               .arg(tex.id)
                               .arg(pixmap.width())
                               .arg(pixmap.height())
                               .arg(tex.filename));
            break;
        }
    }
}

void FPGEditor::onTextureSelected(QListWidgetItem *item)
{
    if (!item) return;
    
    m_selectedTextureId = item->data(Qt::UserRole).toInt();
    updatePreview(m_selectedTextureId);
    m_removeButton->setEnabled(true);
}

void FPGEditor::onBrowseImage()
{
    QString filename = QFileDialog::getOpenFileName(this, 
        tr("Seleccionar Imagen"),
        "",
        tr("Archivos de Imagen (*.png *.jpg *.jpeg *.bmp *.tga)"));
    
    if (!filename.isEmpty()) {
        m_imagePathEdit->setText(filename);
    }
}

void FPGEditor::onAddTexture()
{
    QString imagePath = m_imagePathEdit->text();
    if (imagePath.isEmpty()) return;
    
    int newId = m_textureIDSpinBox->value();
    
    // Check if ID already exists
    for (const auto &tex : m_textures) {
        if (tex.id == newId) {
            QMessageBox::warning(this, tr("Error"),
                               tr("¡El ID de textura %1 ya existe!").arg(newId));
            return;
        }
    }
    
    // Load image
    QImage image(imagePath);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Error"),
                           tr("Error al cargar la imagen: %1").arg(imagePath));
        return;
    }
    
    // Convert to 32-bit ARGB if needed
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }
    
    // Create texture entry
    TextureEntry newTexture;
    newTexture.id = newId;
    newTexture.pixmap = QPixmap::fromImage(image);
    
    QFileInfo fileInfo(imagePath);
    newTexture.filename = fileInfo.fileName();
    
    // Add to lists
    m_textures.append(newTexture);
    m_textureMap[newId] = newTexture.pixmap;
    
    // Update UI
    updateTextureList();
    
    // Clear input and increment ID
    m_imagePathEdit->clear();
    m_textureIDSpinBox->setValue(newId + 1);
    
    // Mark as modified
    setModified(true);
    
    QMessageBox::information(this, tr("Éxito"),
                           tr("¡Textura %1 añadida correctamente!").arg(newId));
}

void FPGEditor::onRemoveTexture()
{
    if (m_selectedTextureId < 0) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Confirmar"),
        tr("¿Eliminar textura %1?").arg(m_selectedTextureId),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Remove from vectors
        for (int i = 0; i < m_textures.size(); i++) {
            if (m_textures[i].id == m_selectedTextureId) {
                m_textures.removeAt(i);
                break;
            }
        }
        
        m_textureMap.remove(m_selectedTextureId);
        
        // Update UI
        updateTextureList();
        m_previewLabel->clear();
        m_previewLabel->setText(tr("Ninguna textura seleccionada"));
        m_infoLabel->clear();
        m_removeButton->setEnabled(false);
        m_selectedTextureId = -1;
        
        // Mark as modified
        setModified(true);
    }
}

void FPGEditor::onSaveFPG()
{
    if (m_fpgPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No hay ningún archivo FPG cargado"));
        return;
    }
    
    if (saveFPGFile(m_fpgPath)) {
        setModified(false);  // Clear modified flag after save
        QMessageBox::information(this, tr("Éxito"), 
                               tr("¡Archivo FPG guardado correctamente!"));
    }
}

void FPGEditor::onSaveFPGAs()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Guardar FPG Como"),
        m_fpgPath.isEmpty() ? "" : m_fpgPath,
        tr("Archivos FPG (*.fpg)"));
    
    if (filename.isEmpty()) {
        return;
    }
    
    // Ensure .fpg extension
    if (!filename.endsWith(".fpg", Qt::CaseInsensitive)) {
        filename += ".fpg";
    }
    
    if (saveFPGFile(filename)) {
        m_fpgPath = filename;
        setModified(false);  // Clear modified flag after save
        setWindowTitle(tr("Editor de FPG - %1").arg(QFileInfo(filename).fileName()));
        QMessageBox::information(this, tr("Éxito"), 
                               tr("¡Archivo FPG guardado correctamente!"));
    }
}

void FPGEditor::onReloadFPG()
{
    loadFPG();
    emit fpgReloaded();
    QMessageBox::information(this, tr("Éxito"), tr("¡FPG recargado!"));
}

bool FPGEditor::saveFPGFile(const QString &filepath)
{
    if (filepath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Ruta de archivo inválida"));
        return false;
    }
    
    // Create custom dialog with checkbox
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Opciones de Guardado"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *label = new QLabel(tr("¿Deseas comprimir el archivo FPG?\n\n"
                                   "Los archivos comprimidos son más pequeños\n"
                                   "pero pueden tardar más en cargarse."));
    layout->addWidget(label);
    
    QCheckBox *compressCheckbox = new QCheckBox(tr("Comprimir con gzip"));
    compressCheckbox->setChecked(false);
    layout->addWidget(compressCheckbox);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    
    bool compress = compressCheckbox->isChecked();
    
    // Save FPG
    bool success = FPGLoader::saveFPG(filepath, m_textures, compress);
    
    if (!success) {
        QMessageBox::warning(this, tr("Error"),
                           tr("Error al guardar el archivo FPG."));
    }
    
    return success;
}

// Animation functions
void FPGEditor::onSelectionChanged()
{
    QList<QListWidgetItem*> selected = m_textureList->selectedItems();
    
    // Update animation frames list
    m_animationFrames.clear();
    for (auto *item : selected) {
        m_animationFrames.append(item->data(Qt::UserRole).toInt());
    }
    
    // Sort by ID
    std::sort(m_animationFrames.begin(), m_animationFrames.end());
    
    // Update UI
    m_frameCountLabel->setText(tr("Frames seleccionados: %1").arg(m_animationFrames.size()));
    m_playButton->setEnabled(m_animationFrames.size() > 1);
    
    // Stop animation if playing
    if (m_isPlaying) {
        onStopAnimation();
    }
    
    // Show first frame if any selected
    if (!m_animationFrames.isEmpty()) {
        m_currentAnimFrame = 0;
        updatePreview(m_animationFrames[0]);
        m_currentFrameLabel->setText(tr("Frame: 1/%1").arg(m_animationFrames.size()));
    } else {
        m_currentFrameLabel->setText(tr("Frame: -/-"));
    }
}

void FPGEditor::onPlayAnimation()
{
    if (m_animationFrames.size() < 2) return;
    
    if (m_isPlaying) {
        // Pause
        m_animationTimer->stop();
        m_isPlaying = false;
        m_playButton->setText(tr("▶ Reproducir"));
        m_stopButton->setEnabled(true);
    } else {
        // Play
        int fps = m_fpsSpinBox->value();
        int interval = 1000 / fps;
        m_animationTimer->setInterval(interval);
        m_animationTimer->start();
        m_isPlaying = true;
        m_playButton->setText(tr("⏸ Pausar"));
        m_stopButton->setEnabled(true);
    }
}

void FPGEditor::onStopAnimation()
{
    m_animationTimer->stop();
    m_isPlaying = false;
    m_currentAnimFrame = 0;
    m_playButton->setText(tr("▶ Reproducir"));
    m_stopButton->setEnabled(false);
    
    // Show first frame
    if (!m_animationFrames.isEmpty()) {
        updatePreview(m_animationFrames[0]);
        m_currentFrameLabel->setText(tr("Frame: 1/%1").arg(m_animationFrames.size()));
    }
}

void FPGEditor::onFPSChanged(int fps)
{
    if (m_isPlaying) {
        int interval = 1000 / fps;
        m_animationTimer->setInterval(interval);
    }
}

void FPGEditor::onAnimationTick()
{
    if (m_animationFrames.isEmpty()) return;
    
    // Advance to next frame
    m_currentAnimFrame = (m_currentAnimFrame + 1) % m_animationFrames.size();
    int textureId = m_animationFrames[m_currentAnimFrame];
    updatePreview(textureId);
    
    // Update label
    m_currentFrameLabel->setText(tr("Frame: %1/%2")
                                .arg(m_currentAnimFrame + 1)
                                .arg(m_animationFrames.size()));
}

void FPGEditor::setModified(bool modified)
{
    m_isModified = modified;
    
    // Update window title to show modified state
    QString title = tr("Editor de FPG");
    if (!m_fpgPath.isEmpty()) {
        title += " - " + QFileInfo(m_fpgPath).fileName();
    }
    if (m_isModified) {
        title += " *";
    }
    setWindowTitle(title);
}

void FPGEditor::closeEvent(QCloseEvent *event)
{
    // Stop animation if playing
    if (m_isPlaying) {
        onStopAnimation();
    }
    
    // Check for unsaved changes
    if (m_isModified) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Cambios sin Guardar"),
            tr("Hay cambios sin guardar en el FPG.\n\n¿Deseas guardar antes de cerrar?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (reply == QMessageBox::Save) {
            // Try to save
            if (!m_fpgPath.isEmpty()) {
                if (!saveFPGFile(m_fpgPath)) {
                    event->ignore();
                    return;
                }
            } else {
                // No path, need Save As
                onSaveFPGAs();
                if (m_isModified) {  // Still modified = user cancelled
                    event->ignore();
                    return;
                }
            }
        } else if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        // Discard = continue closing
    }
    
    // Clear editor state
    m_textures.clear();
    m_textureMap.clear();
    m_animationFrames.clear();
    m_fpgPath.clear();
    m_selectedTextureId = -1;
    m_currentAnimFrame = 0;
    m_isModified = false;
    
    // Clear UI
    m_textureList->clear();
    m_previewLabel->clear();
    m_previewLabel->setText(tr("Ninguna textura seleccionada"));
    m_infoLabel->clear();
    m_frameCountLabel->setText(tr("Frames seleccionados: 0"));
    m_currentFrameLabel->setText(tr("Frame: -/-"));
    
    // Disable buttons
    m_saveButton->setEnabled(false);
    m_saveAsButton->setEnabled(false);
    m_reloadButton->setEnabled(false);
    m_removeButton->setEnabled(false);
    m_playButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    
    // Reset title
    setWindowTitle(tr("Editor de FPG"));
    
    event->accept();
}
