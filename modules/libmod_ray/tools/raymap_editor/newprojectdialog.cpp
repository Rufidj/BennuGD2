#include "newprojectdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("New BennuGD2 Project");
    setMinimumWidth(500);
    
    // Widgets
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("MyGame");
    
    m_locationEdit = new QLineEdit(this);
    QString defaultLocation = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_locationEdit->setText(defaultLocation);
    
    m_browseButton = new QPushButton("Browse...", this);
    
    m_fullPathLabel = new QLabel(this);
    m_fullPathLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    
    m_createButton = new QPushButton("Create", this);
    m_createButton->setDefault(true);
    m_createButton->setEnabled(false);
    
    m_cancelButton = new QPushButton("Cancel", this);
    
    // Layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow("Project Name:", m_nameEdit);
    
    QHBoxLayout *locationLayout = new QHBoxLayout();
    locationLayout->addWidget(m_locationEdit);
    locationLayout->addWidget(m_browseButton);
    formLayout->addRow("Location:", locationLayout);
    
    mainLayout->addLayout(formLayout);
    mainLayout->addSpacing(10);
    
    QLabel *pathLabel = new QLabel("Project will be created at:", this);
    pathLabel->setStyleSheet("QLabel { font-weight: bold; }");
    mainLayout->addWidget(pathLabel);
    mainLayout->addWidget(m_fullPathLabel);
    
    mainLayout->addStretch();
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_createButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);
    
    // Connections
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewProjectDialog::updateFullPath);
    connect(m_locationEdit, &QLineEdit::textChanged, this, &NewProjectDialog::updateFullPath);
    connect(m_browseButton, &QPushButton::clicked, this, &NewProjectDialog::onBrowse);
    connect(m_createButton, &QPushButton::clicked, this, &NewProjectDialog::onAccept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    updateFullPath();
}

void NewProjectDialog::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Project Location",
        m_locationEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        m_locationEdit->setText(dir);
    }
}

void NewProjectDialog::onAccept()
{
    m_projectName = m_nameEdit->text().trimmed();
    m_projectPath = m_locationEdit->text().trimmed();
    
    if (m_projectName.isEmpty()) {
        QMessageBox::warning(this, "Invalid Name", "Please enter a project name.");
        return;
    }
    
    if (m_projectPath.isEmpty()) {
        QMessageBox::warning(this, "Invalid Location", "Please select a project location.");
        return;
    }
    
    // Check if project directory already exists
    QString fullPath = m_projectPath + "/" + m_projectName;
    if (QDir(fullPath).exists()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Directory Exists",
            QString("The directory '%1' already exists.\nDo you want to use it anyway?").arg(fullPath),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    accept();
}

void NewProjectDialog::updateFullPath()
{
    QString name = m_nameEdit->text().trimmed();
    QString location = m_locationEdit->text().trimmed();
    
    if (name.isEmpty() || location.isEmpty()) {
        m_fullPathLabel->setText("<i>Enter project name and location</i>");
        m_createButton->setEnabled(false);
    } else {
        QString fullPath = location + "/" + name;
        m_fullPathLabel->setText(fullPath);
        m_createButton->setEnabled(true);
    }
}
