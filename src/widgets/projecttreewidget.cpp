#include "projecttreewidget.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>

namespace {
constexpr int kKindRole = Qt::UserRole;
constexpr int kPathRole = Qt::UserRole + 1;
}

ProjectTreeWidget::ProjectTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setColumnCount(1);
    setIndentation(8);   // 与原版一致，节点更靠左
    setHeaderHidden(true);
    setupTree();

    connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        if (!item) {
            return;
        }
        emit itemSelected(static_cast<ItemKind>(item->data(0, kKindRole).toInt()),
                          item->data(0, kPathRole).toString(),
                          item->text(0));
    });
}

void ProjectTreeWidget::setupTree()
{
    clear();

    QTreeWidgetItem *root = invisibleRootItem();

    m_projectItem = new QTreeWidgetItem(root);
    m_projectItem->setText(0, QStringLiteral("项目管理"));
    m_projectItem->setIcon(0, QIcon(QStringLiteral(":/icons/icons/xiangmuguanli.png")));
    m_projectItem->setData(0, kKindRole, static_cast<int>(ItemKind::Project));
    // 字号层级与原版一致：根 9.5、分组 8.5、文件组 8
    QFont projectFont = m_projectItem->font(0);
    projectFont.setPointSize(9.5);
    projectFont.setBold(true);
    m_projectItem->setFont(0, projectFont);

    m_taskList = createItem(m_projectItem, QStringLiteral("任务详情"), ItemKind::None, QString());
    m_taskList->setIcon(0, QIcon(QStringLiteral(":/icons/icons/renwuliebiao.png")));
    QFont taskFont = m_taskList->font(0);
    taskFont.setPointSize(8.5);
    taskFont.setBold(true);
    m_taskList->setFont(0, taskFont);

    m_dataFiles = createItem(m_projectItem, QStringLiteral("数据文件"), ItemKind::None, QString());
    m_dataFiles->setIcon(0, QIcon(QStringLiteral(":/icons/icons/shuju_shujuwenjian.png")));
    QFont dataFont = m_dataFiles->font(0);
    dataFont.setPointSize(8.5);
    dataFont.setBold(true);
    m_dataFiles->setFont(0, dataFont);

    m_baselineGroup = createItem(m_dataFiles, QStringLiteral("基准文件"), ItemKind::None, QString());
    m_baselineGroup->setIcon(0, QIcon(QStringLiteral(":/icons/icons/jizunwenjian.png")));
    QFont baselineFont = m_baselineGroup->font(0);
    baselineFont.setPointSize(8);
    m_baselineGroup->setFont(0, baselineFont);

    m_evaluationGroup = createItem(m_dataFiles, QStringLiteral("评估文件"), ItemKind::None, QString());
    m_evaluationGroup->setIcon(0, QIcon(QStringLiteral(":/icons/icons/pingguwenjian - .png")));
    QFont evaluationFont = m_evaluationGroup->font(0);
    evaluationFont.setPointSize(8);
    m_evaluationGroup->setFont(0, evaluationFont);

    m_resultGroup = createItem(m_projectItem, QStringLiteral("结果文件"), ItemKind::None, QString());
    m_resultGroup->setIcon(0, QIcon(QStringLiteral(":/icons/icons/jieguowenjian.png")));
    QFont resultFont = m_resultGroup->font(0);
    resultFont.setPointSize(8.5);
    resultFont.setBold(true);
    m_resultGroup->setFont(0, resultFont);

    m_projectItem->setExpanded(true);
    m_taskList->setExpanded(true);
    m_dataFiles->setExpanded(true);
    m_resultGroup->setExpanded(true);
}

QTreeWidgetItem *ProjectTreeWidget::createItem(QTreeWidgetItem *parent, const QString &text,
                                               ItemKind kind, const QString &path)
{
    auto *item = new QTreeWidgetItem(parent);
    item->setText(0, text);
    item->setData(0, kKindRole, static_cast<int>(kind));
    item->setData(0, kPathRole, path);
    return item;
}

void ProjectTreeWidget::rebuild()
{
    setupTree();
}

void ProjectTreeWidget::setProjectName(const QString &name)
{
    if (m_taskList && m_taskList->childCount() == 0) {
        QTreeWidgetItem *task = createItem(m_taskList, name, ItemKind::None, QString());
        task->setIcon(0, QIcon(QStringLiteral(":/icons/icons/xiangmuguanli.png")));
        m_taskList->setExpanded(true);
    }
}

void ProjectTreeWidget::addDataFile(ItemKind kind, const QString &displayName, const QString &path)
{
    QTreeWidgetItem *group = groupFor(kind);
    if (!group) {
        return;
    }

    // 同一路径不重复添加
    for (int i = 0; i < group->childCount(); ++i) {
        if (group->child(i)->data(0, kPathRole).toString() == path) {
            return;
        }
    }

    const QIcon icon(kind == ItemKind::Baseline
                         ? QStringLiteral(":/icons/icons/jizunwenjian.png")
                         : QStringLiteral(":/icons/icons/pingguwenjian - .png"));
    QTreeWidgetItem *item = createItem(group, displayName, kind, path);
    item->setIcon(0, icon);
    group->setExpanded(true);
}

void ProjectTreeWidget::addResultFile(const QString &displayName, const QString &path)
{
    if (!m_resultGroup) {
        return;
    }
    QTreeWidgetItem *item = createItem(m_resultGroup, displayName, ItemKind::Result, path);
    item->setIcon(0, QIcon(QStringLiteral(":/icons/icons/jieguowenjian.png")));
    m_resultGroup->setExpanded(true);
}

void ProjectTreeWidget::clearResults()
{
    if (!m_resultGroup) {
        return;
    }
    while (m_resultGroup->childCount() > 0) {
        delete m_resultGroup->takeChild(0);
    }
}

bool ProjectTreeWidget::hasDataFile(ItemKind kind) const
{
    QTreeWidgetItem *group = groupFor(kind);
    return group && group->childCount() > 0;
}

int ProjectTreeWidget::resultCount() const
{
    return m_resultGroup ? m_resultGroup->childCount() : 0;
}

QTreeWidgetItem *ProjectTreeWidget::groupFor(ItemKind kind) const
{
    switch (kind) {
    case ItemKind::Baseline: return m_baselineGroup;
    case ItemKind::Evaluation: return m_evaluationGroup;
    case ItemKind::Result: return m_resultGroup;
    default: return nullptr;
    }
}

ProjectTreeWidget::ItemKind ProjectTreeWidget::kindOf(const QModelIndex &index) const
{
    QTreeWidgetItem *item = itemFromIndex(index);
    if (!item) {
        return ItemKind::None;
    }
    return static_cast<ItemKind>(item->data(0, kKindRole).toInt());
}

QString ProjectTreeWidget::pathOf(const QModelIndex &index) const
{
    QTreeWidgetItem *item = itemFromIndex(index);
    return item ? item->data(0, kPathRole).toString() : QString();
}

QString ProjectTreeWidget::nameOf(const QModelIndex &index) const
{
    QTreeWidgetItem *item = itemFromIndex(index);
    return item ? item->text(0) : QString();
}

void ProjectTreeWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QTreeWidgetItem *item = itemAt(event->pos());
    if (!item) {
        return;
    }

    const ItemKind kind = static_cast<ItemKind>(item->data(0, kKindRole).toInt());
    // 只有具体的文件/结果节点可以删除；任务节点代表项目本身，删除即清空项目
    if (kind == ItemKind::None) {
        return;
    }

    QMenu menu(this);
    QAction *openAction = nullptr;
    if (kind == ItemKind::Project) {
        // 打开项目入口放在右键菜单里，不占工具栏位置（保持原界面外观）
        openAction = menu.addAction(QStringLiteral("打开项目…"));
        menu.addSeparator();
    }
    QAction *removeAction = menu.addAction(kind == ItemKind::Project ? QStringLiteral("关闭并清空项目")
                                                                    : QStringLiteral("删除"));
    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) {
        return;
    }
    if (chosen == openAction) {
        emit openProjectRequested();
        return;
    }
    if (chosen != removeAction) {
        return;
    }

    const QString path = item->data(0, kPathRole).toString();
    const QString name = item->text(0);

    if (kind != ItemKind::Project) {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            this, QStringLiteral("确认删除"),
            QStringLiteral("确定要从项目中移除“%1”吗？\n（只从项目中移除，不删除磁盘文件）").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    emit deleteRequested(kind, path, name);
}
