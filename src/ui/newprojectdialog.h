#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);
    ~NewProjectDialog();

    QString getProjectName() const;
    QString getProjectPath() const;

private slots:
    void onBrowseClicked();
    void onOkClicked();
    void onCancelClicked();

private:
    void setupUI();
    void setupConnections();

    QLineEdit *m_projectNameEdit;
    QLineEdit *m_projectPathEdit;
    QPushButton *m_browseButton;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
};

#endif // NEWPROJECTDIALOG_H