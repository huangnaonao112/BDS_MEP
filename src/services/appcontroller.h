#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "appstate.h"
#include "evaluationpipeline.h"
#include "importservice.h"
#include "projectmanager.h"
#include "trajectorydata.h"

class ProjectSession;
class EvaluationService;

// 应用控制器：项目/导入/评估/删除的完整流程都在这里，不依赖任何 QWidget。
//
// MainWindow 只做两件事：
//   1) 用户操作 -> 调用这里的方法；
//   2) 收到 stateChanged/alert/trajectory/report 信号 -> 更新界面。
// 前置条件校验、文案、缓存失效都不再散落在窗口类里。
class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    AppState state() const;

    bool createProject(const QString &name, const QString &parentDirectory);
    bool openProject(const QString &directory);
    void closeProject();

    void importFile(ProjectManager::FileRole role, const QString &sourcePath);
    void evaluate();
    void cancelEvaluation();

    void removeFile(ProjectManager::FileRole role);
    void removeResult(const QString &path);
    void loadTrajectory(const QString &path);

    QString reportHtml();
    QString reportText();

    QString lastError() const { return m_lastError; }

signals:
    void stateChanged(const AppState &state);
    void progressChanged(int percent);          // -1 表示不确定进度
    void statusMessage(const QString &text);
    void logMessage(const QString &text, const QString &level);
    void alertRequested(const QString &title, const QString &text, const QString &level);
    void trajectoryReady(const QString &path, const TrajectoryData &data, bool reveal);
    void reportReady();

private:
    enum class Pending {
        None,
        ImportBaseline,
        ImportEvaluation,
        Plot
    };

    void onImportFinished(const ImportOutcome &outcome);
    void onEvaluationFinished(const ev::EvaluationOutcome &outcome);

    void emitState();
    void setBusy(bool busy, bool cancellable, int progress = 0);
    void clearReport();
    void loadReportFromDisk();
    bool reportExists() const;

    ProjectSession *m_session = nullptr;
    ImportService *m_importer = nullptr;
    EvaluationService *m_evaluator = nullptr;

    Pending m_pending = Pending::None;
    QString m_pendingPath;

    bool m_busy = false;
    bool m_cancellable = false;
    int m_progress = 0;

    QString m_reportHtml;
    QString m_reportText;
    QString m_lastError;
};

#endif // APPCONTROLLER_H
