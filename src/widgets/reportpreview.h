#ifndef REPORTPREVIEW_H
#define REPORTPREVIEW_H

#include <QString>
#include <QWidget>

class QTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QHBoxLayout;

// 报表预览：HTML 预览 + 打印 + 导出 PDF。
//
// 外观与原版 PdfView 保持一致（标题条、5 个按钮、滚动区 + QTextEdit），
// 类名按真实职责命名，避免"叫 PDF 查看器其实只是 QTextEdit"的误导。
class ReportPreview : public QWidget
{
    Q_OBJECT

public:
    explicit ReportPreview(QWidget *parent = nullptr);

    void setHtml(const QString &html);
    void clear();
    QString html() const;

private slots:
    void zoomIn();
    void zoomOut();
    void fitPage();
    void print();
    void download();

private:
    void setupUi();
    bool printTo(const QString &pdfPath);
    void applyButtonStyle();

    QVBoxLayout *m_mainLayout = nullptr;
    QHBoxLayout *m_toolbarLayout = nullptr;

    QPushButton *m_zoomInButton = nullptr;
    QPushButton *m_zoomOutButton = nullptr;
    QPushButton *m_fitPageButton = nullptr;
    QPushButton *m_printButton = nullptr;
    QPushButton *m_downloadButton = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    QTextEdit *m_preview = nullptr;
};

#endif // REPORTPREVIEW_H
