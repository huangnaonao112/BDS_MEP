#include "debugoutput.h"
#include <QDateTime>
#include <QScrollBar>

DebugOutput::DebugOutput(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    appendMessage("调试信息输出区已初始化", "info");
}

void DebugOutput::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 文本编辑区域
    m_textEdit = new QTextEdit();
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFont("Consolas", 9));
    m_textEdit->setStyleSheet("QTextEdit {"
                              "background-color: #f8f8f8;"
                              "border: 2px solid #999999;"
                              "border-radius: 3px;"
                              "padding: 5px;"
                              "selection-background-color: #e6f3ff;"
                              "}"
                              "QScrollBar:vertical {"
                              "border: 1px solid #cccccc;"
                              "background: #f0f0f0;"
                              "width: 12px;"
                              "margin: 0px;"
                              "}"
                              "QScrollBar::handle:vertical {"
                              "background: #d0d0d0;"
                              "min-height: 20px;"
                              "border-radius: 6px;"
                              "}"
                              "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                              "height: 0px;"
                              "}"
                              "QScrollBar:horizontal {"
                              "border: 1px solid #cccccc;"
                              "background: #f0f0f0;"
                              "height: 12px;"
                              "margin: 0px;"
                              "}"
                              "QScrollBar::handle:horizontal {"
                              "background: #d0d0d0;"
                              "min-width: 20px;"
                              "border-radius: 6px;"
                              "}"
                              "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                              "width: 0px;"
                              "}");
    
    m_mainLayout->addWidget(m_textEdit);
}

void DebugOutput::appendMessage(const QString &message, const QString &type)
{
    QString timestamp = getCurrentTimestamp();
    QString formattedMessage;
    
    // 根据消息类型设置颜色
    if (type == "error") {
        formattedMessage = QString("[%1] <span style='color: red; font-weight: bold;'>错误: %2</span><br>").arg(timestamp, message);
    } else if (type == "warning") {
        formattedMessage = QString("[%1] <span style='color: orange; font-weight: bold;'>警告: %2</span><br>").arg(timestamp, message);
    } else if (type == "success") {
        formattedMessage = QString("[%1] <span style='color: green; font-weight: bold;'>成功: %2</span><br>").arg(timestamp, message);
    } else {
        formattedMessage = QString("[%1] %2<br>").arg(timestamp, message);
    }
    
    // 插入消息
    m_textEdit->insertHtml(formattedMessage);
    
    // 自动滚动到底部
    QScrollBar *scrollBar = m_textEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

QString DebugOutput::getCurrentTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}