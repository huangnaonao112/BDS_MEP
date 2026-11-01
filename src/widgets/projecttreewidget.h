#ifndef PROJECTTREEWIDGET_H
#define PROJECTTREEWIDGET_H

#include <QTreeWidget>

// 任务树。
//
// 旧版把"文件路径"塞在节点的显示文本里（"xxx.pos (POS基准)"），点击时再用
// 括号截断反推路径——文件改名或路径里带括号就查不到。现在路径存在
// Qt::UserRole 里，节点类型用 ItemKind 标识。
class ProjectTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    enum class ItemKind {
        None,
        Project,
        Baseline,
        Evaluation,
        Result
    };
    Q_ENUM(ItemKind)

    explicit ProjectTreeWidget(QWidget *parent = nullptr);

    // 注意：不能叫 reset()——QAbstractItemView::reset() 是虚槽，模型重置时会被 Qt 调用，
    // 同名会形成 clear()->modelReset->reset()->clear() 的无限递归（曾导致启动即崩）。
    void rebuild();

    void setProjectName(const QString &name);
    void addDataFile(ItemKind kind, const QString &displayName, const QString &path);
    void addResultFile(const QString &displayName, const QString &path);
    void clearResults();

    bool hasDataFile(ItemKind kind) const;
    int resultCount() const;

    ItemKind kindOf(const QModelIndex &index) const;
    QString pathOf(const QModelIndex &index) const;
    QString nameOf(const QModelIndex &index) const;

signals:
    void itemSelected(ProjectTreeWidget::ItemKind kind, const QString &path, const QString &name);
    void deleteRequested(ProjectTreeWidget::ItemKind kind, const QString &path, const QString &name);
    void openProjectRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setupTree();
    QTreeWidgetItem *createItem(QTreeWidgetItem *parent, const QString &text,
                                ItemKind kind, const QString &path);
    QTreeWidgetItem *groupFor(ItemKind kind) const;

    QTreeWidgetItem *m_taskList = nullptr;
    QTreeWidgetItem *m_dataFiles = nullptr;
    QTreeWidgetItem *m_baselineGroup = nullptr;
    QTreeWidgetItem *m_evaluationGroup = nullptr;
    QTreeWidgetItem *m_resultGroup = nullptr;
    QTreeWidgetItem *m_projectItem = nullptr;
};

#endif // PROJECTTREEWIDGET_H
