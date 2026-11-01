#include "filetableview.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>

FileTableView::FileTableView(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void FileTableView::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 表格视图
    m_tableView = new QTableView();
    m_model = new QStandardItemModel(this);
    m_tableView->setModel(m_model);
    
    // 设置固定的表格字段
    QStringList headers = {"开始时间", "结束时间", "采样间隔", "历元数据", "完整率", "固定率", "文件类型"};
    m_model->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->horizontalHeader()->setHighlightSections(false);
    m_tableView->verticalHeader()->setVisible(false);
    
    // 设置列宽调整模式 - 允许手动拖动调整
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    
    // 设置表格视图的大小调整策略
    m_tableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 禁用排序功能
    m_tableView->setSortingEnabled(false);
    
    // 初始列宽平均分布
    // 先获取表格的初始宽度
    int initialWidth = this->width();
    // 计算每列的平均宽度
    int columnCount = headers.size();
    if (initialWidth > 0 && columnCount > 0) {
        int columnWidth = initialWidth / columnCount;
        // 为每列设置初始宽度
        for (int i = 0; i < columnCount; ++i) {
            m_tableView->setColumnWidth(i, columnWidth);
        }
    }
    
    // 美化表格样式
    m_tableView->setStyleSheet("QTableView {"
                               "background-color: white;"
                               "gridline-color: #e0e0e0;"
                               "border: 1px solid #cccccc;"
                               "selection-background-color: #e6f3ff;"
                               "}"
                               "QTableView::item {"
                               "padding: 5px;"
                               "}"
                               "QHeaderView::section {"
                               "background-color: #f0f0f0;"
                               "padding: 5px;"
                               "border: 1px solid #cccccc;"
                               "font-weight: bold;"
                               "}");
    
    m_tableView->setAlternatingRowColors(true);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setDefaultSectionSize(25);
    
    m_mainLayout->addWidget(m_tableView);
}

void FileTableView::setFiles(const QVector<ProjectManager::FileRecord> &files)
{
    // 清空现有数据，但保留固定表头
    m_model->removeRows(0, m_model->rowCount());

    for (const ProjectManager::FileRecord &record : files) {
        const TrackMeta &meta = record.meta;
        const QStringList row = {
            meta.startTime.isEmpty() ? QStringLiteral("--") : meta.startTime,
            meta.endTime.isEmpty() ? QStringLiteral("--") : meta.endTime,
            QStringLiteral("%1 s").arg(meta.intervalSeconds, 0, 'f', 2),
            QString::number(meta.epochCount),
            QStringLiteral("%1%").arg(meta.completenessPercent, 0, 'f', 1),
            QStringLiteral("%1%").arg(meta.fixRatePercent, 0, 'f', 1),
            record.role == ProjectManager::FileRole::Baseline ? QStringLiteral("基准")
                                                              : QStringLiteral("评估"),
        };

        QList<QStandardItem *> items;
        items.reserve(row.size());
        for (const QString &cell : row) {
            items.append(new QStandardItem(cell));
        }
        m_model->appendRow(items);
    }

    applyColumnWidths();
}

void FileTableView::applyColumnWidths()
{
    const int tableWidth = width();
    const int columnCount = m_model->columnCount();
    if (tableWidth <= 0 || columnCount <= 0) {
        return;
    }
    const int columnWidth = tableWidth / columnCount;
    for (int i = 0; i < columnCount; ++i) {
        m_tableView->setColumnWidth(i, columnWidth);
    }
}

void FileTableView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyColumnWidths();
}

