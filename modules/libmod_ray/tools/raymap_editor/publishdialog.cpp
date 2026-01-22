#include "publishdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QStackedWidget>
#include <QGroupBox>
#include <QTimer>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QFrame>

PublishDialog::PublishDialog(ProjectData *project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    setWindowTitle(tr("Publicar Proyecto"));
    resize(550, 450);
    setupUI();
    
    // Load saved settings
    if (m_project) {
        m_packageNameEdit->setText(m_project->packageName);
        if (m_packageNameEdit->text().isEmpty()) m_packageNameEdit->setText("com.example.game");
        
        m_iconPathEdit->setText(m_project->iconPath);
    }
}

void PublishDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Platform Selection
    QFormLayout *topLayout = new QFormLayout();
    m_platformCombo = new QComboBox();
    m_platformCombo->addItem(tr("Linux (64-bit)"), Publisher::Linux);
    m_platformCombo->addItem(tr("Windows (64-bit)"), Publisher::Windows);
    // Android disabled - BennuGD2 doesn't support Android compilation yet
    // m_platformCombo->addItem(tr("Android"), Publisher::Android);
    
    topLayout->addRow(tr("Plataforma Destino:"), m_platformCombo);
    
    // Output Path
    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit();
    QPushButton *browseBtn = new QPushButton("...");
    pathLayout->addWidget(m_outputPathEdit);
    pathLayout->addWidget(browseBtn);
    topLayout->addRow(tr("Carpeta de Salida:"), pathLayout);
    
    // Icon Selection (Common for all platforms)
    m_iconPathEdit = new QLineEdit();
    QHBoxLayout *iconLayout = new QHBoxLayout();
    iconLayout->addWidget(m_iconPathEdit);
    QPushButton *iconBrowseBtn = new QPushButton("...");
    iconLayout->addWidget(iconBrowseBtn);
    topLayout->addRow(tr("Icono (.png):"), iconLayout);
    
    connect(iconBrowseBtn, &QPushButton::clicked, this, &PublishDialog::onBrowseIcon);
    
    mainLayout->addLayout(topLayout);

    // Stacked Options
    QStackedWidget *optionsStack = new QStackedWidget();
    
    // === LINUX OPTIONS ===
    m_linuxOptions = new QWidget();
    QVBoxLayout *linuxLayout = new QVBoxLayout(m_linuxOptions);
    QGroupBox *linuxGroup = new QGroupBox(tr("Opciones de Linux"));
    QVBoxLayout *linuxGroupLayout = new QVBoxLayout(linuxGroup);
    
    m_chkLinuxArchive = new QCheckBox(tr("Crear paquete comprimido (.tar.gz)"));
    m_chkLinuxArchive->setChecked(true);
    m_chkLinuxArchive->setToolTip(tr("Incluye ejecutable, librerías y script de lanzamiento"));
    
    m_chkLinuxAppImage = new QCheckBox(tr("Crear AppImage"));
    
    QHBoxLayout *appImageLayout = new QHBoxLayout();
    appImageLayout->addWidget(m_chkLinuxAppImage);
    
    QPushButton *dlAppImageBtn = new QPushButton(QIcon::fromTheme("download"), tr("Descargar Tool"));
    dlAppImageBtn->setToolTip(tr("Descargar appimagetool si no está instalado"));
    connect(dlAppImageBtn, &QPushButton::clicked, this, &PublishDialog::onDownloadAppImageTool);
    appImageLayout->addWidget(dlAppImageBtn);
    
    // Check availability
    QString systemTool = QStandardPaths::findExecutable("appimagetool");
    QString localTool = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/bin/appimagetool";
    
    if (!systemTool.isEmpty()) {
        m_appImageToolPath = systemTool;
    } else if (QFile::exists(localTool)) {
        if (QFileInfo(localTool).size() > 1024 * 1024) { // Valid if > 1MB
            m_appImageToolPath = localTool;
        } else {
            // File exists but is corrupt/empty. Delete it to allow redownload.
            QFile::remove(localTool);
        }
    }
    
    bool hasAppImageTool = !m_appImageToolPath.isEmpty();
    m_chkLinuxAppImage->setEnabled(hasAppImageTool);
    m_chkLinuxAppImage->setText(hasAppImageTool ? tr("Crear AppImage (Disponible)") : tr("Crear AppImage (Falta herramienta)"));
    dlAppImageBtn->setVisible(!hasAppImageTool);

    linuxGroupLayout->addLayout(appImageLayout);
    linuxLayout->addWidget(linuxGroup);
    linuxLayout->addStretch();
    
    // === WINDOWS OPTIONS ===
    m_windowsOptions = new QWidget();
    QVBoxLayout *windowsLayout = new QVBoxLayout(m_windowsOptions);
    QGroupBox *windowsGroup = new QGroupBox(tr("Opciones de Windows"));
    QVBoxLayout *windowsGroupLayout = new QVBoxLayout(windowsGroup);
    
    // Standalone executable option (Always enabled now via stub concatenation)
    m_chkWindowsStandalone = new QCheckBox(tr("Crear ejecutable autónomo (.exe único)"));
    m_chkWindowsStandalone->setChecked(true);
    m_chkWindowsStandalone->setEnabled(true);
    m_chkWindowsStandalone->setToolTip(tr("Un solo archivo .exe con todo embebido (Recomendado)"));
    
    QHBoxLayout *standaloneLayout = new QHBoxLayout();
    standaloneLayout->addWidget(m_chkWindowsStandalone);
    
    // MinGW install button removed as it's no longer strictly required for standalone exe
    m_installMingwBtn = nullptr;
    
    windowsGroupLayout->addLayout(standaloneLayout);
    
    // Check for 7z (for SFX)
    QString sevenZExe = findToolPath("7z");
    if (sevenZExe.isEmpty()) {
        sevenZExe = findToolPath("7za");
    }
    bool has7z = !sevenZExe.isEmpty();
    
    // SFX option
    m_chkWindowsSfx = new QCheckBox(tr("Crear auto-extraíble 7-Zip"));
    m_chkWindowsSfx->setChecked(false);
    m_chkWindowsSfx->setEnabled(has7z);
    m_chkWindowsSfx->setToolTip(tr("Ejecutable que se auto-extrae y ejecuta"));
    
    QHBoxLayout *sfxLayout = new QHBoxLayout();
    sfxLayout->addWidget(m_chkWindowsSfx);
    
    m_install7zBtn = nullptr;
    if (!has7z) {
        m_install7zBtn = new QPushButton(QIcon::fromTheme("software-install"), tr("Instalar 7-Zip"));
        m_install7zBtn->setToolTip(tr("Instalar herramienta de compresión 7-Zip"));
        connect(m_install7zBtn, &QPushButton::clicked, this, [this]() {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Instalar 7-Zip"));
            msgBox.setText(tr("Para crear ejecutables auto-extraíbles, necesitas instalar p7zip.\n\n"
                             "Ejecuta este comando en la terminal:"));
            msgBox.setDetailedText("sudo apt install p7zip-full");
            msgBox.setIcon(QMessageBox::Information);
            
            QPushButton *copyBtn = msgBox.addButton(tr("Copiar Comando"), QMessageBox::ActionRole);
            QPushButton *refreshBtn = msgBox.addButton(tr("Refrescar"), QMessageBox::ActionRole);
            msgBox.addButton(QMessageBox::Ok);
            
            msgBox.exec();
            
            if (msgBox.clickedButton() == copyBtn) {
                QClipboard *clipboard = QApplication::clipboard();
                clipboard->setText("sudo apt install p7zip-full");
                QMessageBox::information(this, tr("Copiado"), 
                    tr("Comando copiado al portapapeles.\nPégalo en la terminal y presiona Enter."));
            } else if (msgBox.clickedButton() == refreshBtn) {
                refreshWindowsTools();
            }
        });
        sfxLayout->addWidget(m_install7zBtn);
        m_chkWindowsSfx->setText(tr("Auto-extraíble (Requiere 7-Zip)"));
    }
    
    windowsGroupLayout->addLayout(sfxLayout);
    
    // ZIP option (always available)
    m_chkWindowsZip = new QCheckBox(tr("Crear archivo ZIP"));
    m_chkWindowsZip->setChecked(true);
    m_chkWindowsZip->setToolTip(tr("Comprime la carpeta en un archivo .zip para fácil distribución"));
    windowsGroupLayout->addWidget(m_chkWindowsZip);
    
    // Add separator
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    windowsGroupLayout->addWidget(separator);
    
    QLabel *windowsInfo = new QLabel(tr(
        "<b>Nota:</b> Se generará siempre una carpeta con:<br>"
        "• bgdi.exe (motor BennuGD)<br>"
        "• .dcb (código compilado)<br>"
        "• .bat y .vbs (launchers)<br>"
        "• DLLs y assets<br><br>"
        "<b>Requisito:</b> Coloca los binarios de Windows en <tt>runtime/win64/</tt>"));
    windowsInfo->setWordWrap(true);
    windowsInfo->setStyleSheet("color: #666; font-size: 10pt;");
    windowsGroupLayout->addWidget(windowsInfo);
    
    windowsLayout->addWidget(windowsGroup);
    windowsLayout->addStretch();
    
    // === ANDROID OPTIONS ===
    m_androidOptions = new QWidget();
    QVBoxLayout *androidLayout = new QVBoxLayout(m_androidOptions);
    QGroupBox *androidGroup = new QGroupBox(tr("Opciones de Android"));
    QFormLayout *androidForm = new QFormLayout(androidGroup);
    
    m_packageNameEdit = new QLineEdit();
    m_packageNameEdit->setPlaceholderText("com.company.game");
    
    // Icon selection moved to top
    
    m_chkAndroidProject = new QCheckBox(tr("Generar Proyecto Android Studio"));
    m_chkAndroidProject->setChecked(true);
    m_chkAndroidProject->setEnabled(false); // Always generate project
    
    m_chkAndroidAPK = new QCheckBox(tr("Intentar compilar APK (Requiere SDK/NDK)"));
    m_chkAndroidAPK->setChecked(false);
    
    QPushButton *ndkHelpBtn = new QPushButton(QIcon::fromTheme("download"), tr("Descargar NDK"));
    ndkHelpBtn->setToolTip(tr("Descargar e instalar NDK r26b")); // Recommended version
    connect(ndkHelpBtn, &QPushButton::clicked, this, &PublishDialog::onDownloadNDK);
    
    // Check if NDK exists
    QString ndkHome = qgetenv("ANDROID_NDK");
    if (ndkHome.isEmpty()) ndkHome = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Android/Sdk/ndk/27.0.12077973"; // Expected by BennuGD2
    
    bool hasNDK = QDir(ndkHome).exists();
    if (hasNDK) {
        ndkHelpBtn->setText(tr("NDK Detectado"));
        ndkHelpBtn->setEnabled(false);
        ndkHelpBtn->setIcon(QIcon::fromTheme("emblem-ok-symbolic"));
    }
    
    QHBoxLayout *apkLayout = new QHBoxLayout();
    apkLayout->addWidget(m_chkAndroidAPK);
    apkLayout->addWidget(ndkHelpBtn);
    apkLayout->addStretch();
    
    androidForm->addRow(tr("Package Name:"), m_packageNameEdit);
    // Icon removed from here
    androidForm->addRow(m_chkAndroidProject);
    androidForm->addRow(apkLayout); // Layout with button
    
    androidLayout->addWidget(androidGroup);
    
    QLabel *androidInfo = new QLabel(tr("Nota: Se generará un proyecto completo con Gradle.\n"
                                        "El editor copiará las librerías si se encuentran."));
    androidInfo->setWordWrap(true);
    androidInfo->setStyleSheet("color: #888; font-style: italic;");
    androidLayout->addWidget(androidInfo);
    
    androidLayout->addStretch();
    
    optionsStack->addWidget(m_linuxOptions);
    optionsStack->addWidget(m_windowsOptions);
    optionsStack->addWidget(m_androidOptions);
    
    mainLayout->addWidget(optionsStack);
    
    // ... rest of UI setup ... (ProgressBar, Buttons)
    
    // Progress Bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_closeButton = new QPushButton(tr("Cancelar"));
    m_publishButton = new QPushButton(tr("Publicar"));
    m_publishButton->setDefault(true);
    m_publishButton->setStyleSheet("font-weight: bold; padding: 5px 20px;");
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_closeButton);
    btnLayout->addWidget(m_publishButton);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(browseBtn, &QPushButton::clicked, this, &PublishDialog::onBrowseOutput);
    // iconBrowseBtn connection moved up
    
    // Sync Combo with Stack
    connect(m_platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            optionsStack, &QStackedWidget::setCurrentIndex);
    connect(m_platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PublishDialog::onPlatformChanged);

    connect(m_publishButton, &QPushButton::clicked, this, &PublishDialog::onPublish);
    
    // Publisher connections
    connect(&m_publisher, &Publisher::progress, m_progressBar, &QProgressBar::setValue);
    connect(&m_publisher, &Publisher::progress, [this](int, QString msg){
        m_progressBar->setFormat("%p% - " + msg);
    });
    
    connect(&m_publisher, &Publisher::finished, [this](bool success, QString msg){
        m_publishButton->setEnabled(true);
        m_closeButton->setEnabled(true);
        m_progressBar->setVisible(false);
        if (success) {
            QMessageBox::information(this, tr("Publicación Exitosa"), msg);
            accept();
        } else {
            QMessageBox::critical(this, tr("Error de Publicación"), msg);
        }
    });
}

void PublishDialog::onPublish()
{
    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Aviso"), tr("Por favor selecciona una carpeta de salida."));
        return;
    }

    // Save project metadata back to project data (to be persisted later)
    if (m_project) {
        m_project->packageName = m_packageNameEdit->text();
        m_project->iconPath = m_iconPathEdit->text();
    }

    Publisher::PublishConfig config;
    config.platform = (Publisher::Platform)m_platformCombo->currentData().toInt();
    config.outputPath = m_outputPathEdit->text();
    config.iconPath = m_iconPathEdit->text(); // Always set icon path
    
    if (config.platform == Publisher::Linux) {
        config.generateAppImage = m_chkLinuxAppImage->isChecked();
        config.appImageToolPath = m_appImageToolPath;
    } else if (config.platform == Publisher::Windows) {
        config.generateStandalone = m_chkWindowsStandalone->isChecked();
        config.generateSfx = m_chkWindowsSfx->isChecked();
        config.generateZip = m_chkWindowsZip->isChecked();
    } else if (config.platform == Publisher::Android) {
        config.packageName = m_packageNameEdit->text();
        // config.iconPath set above
        config.fullProject = m_chkAndroidProject->isChecked();

        config.generateAPK = m_chkAndroidAPK->isChecked();
        
        // Use environment variable for NDK if set
        QString envNdk = qgetenv("ANDROID_NDK_HOME");
        if (!envNdk.isEmpty()) config.ndkPath = envNdk;
        
        if (config.packageName.isEmpty()) {
             QMessageBox::warning(this, tr("Aviso"), tr("El nombre de paquete es obligatorio para Android."));
             return;
        }
        
        // Basic validation
        QRegularExpression regex("^[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)+$");
        if (!regex.match(config.packageName).hasMatch()) {
             QMessageBox::warning(this, tr("Aviso"), tr("El nombre de paquete debe tener formato 'com.empresa.juego'."));
             return;
        }
    }
    
    m_publishButton->setEnabled(false);
    m_closeButton->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("Iniciando...");
    
    // Execute asynchronously to keep UI alive (simulated via timer for now)
    QTimer::singleShot(100, [this, config](){
        m_publisher.publish(*m_project, config);
    });
}

void PublishDialog::onDownloadAppImageTool()
{
    QString url = "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage";
    QString dest = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/bin/appimagetool";
    QDir().mkpath(QFileInfo(dest).path()); // Ensure ~/.local/bin exists
    
    DownloadDialog dlg(url, dest, tr("Descargando AppImageTool"), false, this);
    if (dlg.start()) {
        QMessageBox::information(this, tr("Éxito"), tr("Herramienta descargada en %1\nAsegúrate de que esta ruta esté en tu PATH.").arg(dest));
        m_appImageToolPath = dest; // Store path
        m_chkLinuxAppImage->setEnabled(true);
        m_chkLinuxAppImage->setText(tr("Crear AppImage (Disponible)"));
    }
}

void PublishDialog::onDownloadNDK()
{
    // NDK 27.0.12077973 (as expected by BennuGD2's build-android-deps.sh)
    QString url = "https://dl.google.com/android/repository/android-ndk-r27-linux.zip";
    QString androidSdk = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Android/Sdk";
    QString destZip = androidSdk + "/ndk-bundle.zip";
    
    QDir().mkpath(androidSdk);
    
    // Use true for autoUnzip
    DownloadDialog dlg(url, destZip, tr("Descargando NDK 27 (1GB+)"), true, this);
    if (dlg.start()) {
        // Unzip creates "android-ndk-r27" folder inside androidSdk
        QString extractedPath = androidSdk + "/android-ndk-r27";
        
        // Create symlink to match expected path structure
        QString expectedPath = androidSdk + "/ndk/27.0.12077973";
        QDir().mkpath(androidSdk + "/ndk");
        
        // Remove old symlink if exists
        QFile::remove(expectedPath);
        
        // Create symlink (or copy if symlink fails)
        if (!QFile::link(extractedPath, expectedPath)) {
            // Fallback: just set env to extracted path
            qputenv("ANDROID_NDK", extractedPath.toUtf8());
        } else {
            qputenv("ANDROID_NDK", expectedPath.toUtf8());
        }
        
        QMessageBox::information(this, tr("NDK Instalado"), 
            tr("El NDK 27 se ha instalado en:\n%1\n\nSe ha configurado ANDROID_NDK para esta sesión.\n\nPara uso permanente, agrega a ~/.bashrc:\nexport ANDROID_NDK=%1").arg(extractedPath));
           
        // Refresh could be better but this is sufficient for now
    }
}

void PublishDialog::onBrowseOutput()
{
    QString initialDir = m_project ? m_project->path : QDir::homePath();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Seleccionar Carpeta de Salida"), 
                                                  initialDir);
    if (!dir.isEmpty()) {
        m_outputPathEdit->setText(dir);
    }
}

void PublishDialog::onBrowseIcon()
{
    QString initialDir = m_project ? m_project->path : QDir::homePath();
    QString file = QFileDialog::getOpenFileName(this, tr("Seleccionar Icono"), 
                                              initialDir, "Images (*.png)");
    if (!file.isEmpty()) {
        m_iconPathEdit->setText(file);
    }
}

void PublishDialog::onPlatformChanged(int index)
{
    // Update logic if needed
}

void PublishDialog::refreshWindowsTools()
{
    // Standalone executable is always available via concatenation method
    m_chkWindowsStandalone->setEnabled(true);
    m_chkWindowsStandalone->setText(tr("Crear ejecutable autónomo (.exe único)"));
    if (m_installMingwBtn) {
        m_installMingwBtn->setVisible(false);
    }
    
    // Re-detect 7z using robust search
    QString sevenZExe = findToolPath("7z");
    if (sevenZExe.isEmpty()) {
        sevenZExe = findToolPath("7za");
    }
    bool has7z = !sevenZExe.isEmpty();
    
    // Update SFX checkbox
    m_chkWindowsSfx->setEnabled(has7z);
    
    if (has7z) {
        m_chkWindowsSfx->setText(tr("Crear auto-extraíble 7-Zip"));
        if (m_install7zBtn) {
            m_install7zBtn->setVisible(false);
        }
    } else {
        m_chkWindowsSfx->setText(tr("Auto-extraíble (Requiere 7-Zip)"));
        if (m_install7zBtn) {
            m_install7zBtn->setVisible(true);
        }
    }
    
    // Show success message
    QString message = tr("Herramientas detectadas:\n\n");
    message += tr("✓ Generador autónomo integrado\n");
    message += has7z ? tr("✓ 7-Zip instalado\n") : tr("✗ 7-Zip no encontrado\n");
    
    QMessageBox::information(this, tr("Detección de Herramientas"), message);
}

QString PublishDialog::findToolPath(const QString &toolName)
{
    // 1. Try standard path lookup
    QString path = QStandardPaths::findExecutable(toolName);
    if (!path.isEmpty()) return path;
    
    // 2. Try implicit common paths
    QStringList paths = {
        "/usr/bin/" + toolName,
        "/usr/local/bin/" + toolName,
        "/bin/" + toolName
    };
    
    for (const QString &p : paths) {
        if (QFile::exists(p)) {
            // Check verify it's executable
            QFileInfo info(p);
            if (info.isExecutable()) {
                qDebug() << "Found tool manually at:" << p;
                return p;
            }
        }
    }
    
    return QString();
}
