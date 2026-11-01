#ifndef FILETABLEVIEW_H
#define FILETABLEVIEW_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

#include "projectmanager.h"

class FileTableView : public QWidget
{
    Q_OBJECT

public:
    explicit FileTableView(QWidget *parent = nullptr);

    // 直接吃项目文件记录：行内容/顺序由控件自己负责，窗口不再拼 QStringList
    void setFiles(const QVector<ProjectManager::FileRecord> &files);

    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUI();
    void applyColumnWidths();

    QVBoxLayout *m_mainLayout;
    QTableView *m_tableView;
    QStandardItemModel *m_model;
};

#endif // FILETABLEVIEW_H
