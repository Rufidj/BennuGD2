#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);
    
    QString getProjectName() const { return m_projectName; }
    QString getProjectPath() const { return m_projectPath; }

private slots:
    void onBrowse();
    void onAccept();
    void updateFullPath();

private:
    QLineEdit *m_nameEdit;
    QLineEdit *m_locationEdit;
    QLabel *m_fullPathLabel;
    QPushButton *m_browseButton;
    QPushButton *m_createButton;
    QPushButton *m_cancelButton;
    
    QString m_projectName;
    QString m_projectPath;
};

#endif // NEWPROJECTDIALOG_H
