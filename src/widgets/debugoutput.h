#ifndef DEBUGOUTPUT_H
#define DEBUGOUTPUT_H

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>

class DebugOutput : public QWidget
{
    Q_OBJECT

public:
    explicit DebugOutput(QWidget *parent = nullptr);
    
    void appendMessage(const QString &message, const QString &type = "info");

private:
    void setupUI();
    QString getCurrentTimestamp();
    
    QVBoxLayout *m_mainLayout;
    QTextEdit *m_textEdit;
};

#endif // DEBUGOUTPUT_H