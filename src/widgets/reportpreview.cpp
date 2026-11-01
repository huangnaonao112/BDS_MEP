#include "reportpreview.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScrollArea>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

ReportPreview::ReportPreview(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    clear();
}

void ReportPreview::applyButtonStyle()
{
    // 与原版按钮样式一致
    const QString buttonStyle = QStringLiteral(
        "QPushButton {"
        "background-color: #f0f0f0;"
        "border: 1px solid #cccccc;"
        "border-radius: 3px;"
        "padding: 5px 10px;"
        "font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #e0e0e0; }"
        "QPushButton:pressed { background-color: #d0d0d0; }");

    for (QPushButton *button : {m_zoomInButton, m_zoomOutButton, m_fitPageButton,
                                m_printButton, m_downloadButton}) {
        button->setStyleSheet(buttonStyle);
    }
}

void ReportPreview::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(5);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);

    QLabel *titleLabel = new QLabel(QStringLiteral("报表预览视图 - PDF展示"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 16px; color: #333333; padding: 8px; "
        "background-color: #f0f0f0; border-radius: 3px;"));
    titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(titleLabel);

    m_toolbarLayout = new QHBoxLayout();

    m_zoomInButton = new QPushButton(QStringLiteral("缩放+"), this);
    m_zoomOutButton = new QPushButton(QStringLiteral("缩放-"), this);
    m_fitPageButton = new QPushButton(QStringLiteral("适合页面"), this);
    m_printButton = new QPushButton(QStringLiteral("打印"), this);
    m_downloadButton = new QPushButton(QStringLiteral("下载"), this);

    applyButtonStyle();

    m_toolbarLayout->addWidget(m_zoomInButton);
    m_toolbarLayout->addWidget(m_zoomOutButton);
    m_toolbarLayout->addWidget(m_fitPageButton);
    m_toolbarLayout->addWidget(m_printButton);
    m_toolbarLayout->addWidget(m_downloadButton);
    m_toolbarLayout->addStretch();

    m_mainLayout->addLayout(m_toolbarLayout);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background-color: white; border: 1px solid #cccccc; border-radius: 3px; }"));

    m_preview = new QTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setFont(QFont(QStringLiteral("宋体"), 10));
    m_preview->setStyleSheet(QStringLiteral("QTextEdit { background-color: white; border: none; }"));

    m_scrollArea->setWidget(m_preview);
    m_mainLayout->addWidget(m_scrollArea);

    connect(m_zoomInButton, &QPushButton::clicked, this, &ReportPreview::zoomIn);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &ReportPreview::zoomOut);
    connect(m_fitPageButton, &QPushButton::clicked, this, &ReportPreview::fitPage);
    connect(m_printButton, &QPushButton::clicked, this, &ReportPreview::print);
    connect(m_downloadButton, &QPushButton::clicked, this, &ReportPreview::download);
}

void ReportPreview::setHtml(const QString &html)
{
    m_preview->setHtml(html);
}

void ReportPreview::clear()
{
    // 原版空态文案，保持视觉一致
    m_preview->setHtml(QStringLiteral(
        "<div style=\"font-family: '宋体'; font-size: 12pt; text-align: center;\">"
        "<h1>数据评估报告</h1>"
        "<p>请先执行数据评估，然后查看报告</p>"
        "</div>"));
}

QString ReportPreview::html() const
{
    return m_preview->toHtml();
}

void ReportPreview::zoomIn()
{
    QFont font = m_preview->font();
    const int newSize = font.pointSize() + 1;
    if (newSize <= 24) {
        font.setPointSize(newSize);
        m_preview->setFont(font);
    }
}

void ReportPreview::zoomOut()
{
    QFont font = m_preview->font();
    const int newSize = font.pointSize() - 1;
    if (newSize >= 8) {
        font.setPointSize(newSize);
        m_preview->setFont(font);
    }
}

void ReportPreview::fitPage()
{
    QFont font = m_preview->font();
    font.setPointSize(10);
    m_preview->setFont(font);
}

bool ReportPreview::printTo(const QString &pdfPath)
{
    QPrinter printer(QPrinter::HighResolution);
    if (pdfPath.isEmpty()) {
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
    } else {
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(pdfPath);
    }

    QTextDocument *document = m_preview->document();
#if defined(QT6)
    document->setPageSize(printer.pageLayout().paintRectPixels(printer.resolution()).size());
#else
    document->setPageSize(printer.pageRect(QPrinter::Point).size());
#endif
    document->print(&printer);
    document->setPageSize(QSizeF());
    return true;
}

void ReportPreview::print()
{
    printTo(QString());
}

void ReportPreview::download()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存PDF报告"), QString(), QStringLiteral("PDF文件 (*.pdf)"));
    if (fileName.isEmpty()) {
        return;
    }

    if (printTo(fileName)) {
        QMessageBox::information(this, QStringLiteral("下载"),
                                 QStringLiteral("PDF报告已保存到: %1").arg(fileName));
    }
}
