#ifndef APPSTATE_H
#define APPSTATE_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "projectmanager.h"

// 主窗口需要渲染的全部状态。控制器只发这个值对象，视图只读它。
// 这样 MainWindow 里不再有 "如果导入成功要清哪个缓存" 之类的分支。
struct AppState {
    bool hasProject = false;
    QString projectName;
    QString projectDirectory;

    QVector<ProjectManager::FileRecord> files;
    QStringList resultFiles;

    bool busy = false;
    bool cancellable = false;
    bool hasReport = false;
};

#endif // APPSTATE_H
