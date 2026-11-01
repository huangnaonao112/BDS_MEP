#include "appcontroller.h"

#include "evaluationservice.h"
#include "projectsession.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {

bool readTextFile(const QString &path, QString *content)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
#if !defined(QT6)
    in.setCodec("UTF-8");
#endif
    *content = in.readAll();
    return true;
}

QString roleName(ProjectManager::FileRole role)
{
    return role == ProjectManager::FileRole::Baseline ? QStringLiteral("基准文件")
                                                      : QStringLiteral("评估文件");
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_session = new ProjectSession(this);
    m_importer = new ImportService(this);
    m_evaluator = new EvaluationService(this);

    connect(m_importer, &ImportService::messageLogged, this,
            [this](const QString &text, const QString &level) { emit logMessage(text, level); });
    connect(m_importer, &ImportService::finished, this, &AppController::onImportFinished);

    connect(m_evaluator, &EvaluationService::messageLogged, this,
            [this](const QString &text, const QString &level) { emit logMessage(text, level); });
    connect(m_evaluator, &EvaluationService::progressChanged, this,
            [this](int percent, const QString &message) {
                m_progress = percent;
                emit progressChanged(percent);
                if (!message.isEmpty()) {
                    emit statusMessage(message);
                }
            });
    connect(m_evaluator, &EvaluationService::finished, this,
            &AppController::onEvaluationFinished);

    connect(m_session, &ProjectSession::filesChanged, this, &AppController::emitState);
    connect(m_session, &ProjectSession::resultsChanged, this, &AppController::emitState);
    connect(m_session, &ProjectSession::projectClosed, this, [this]() {
        clearReport();
        emitState();
    });
}

AppController::~AppController() = default;

AppState AppController::state() const
{
    AppState state;
    state.hasProject = m_session->hasProject();
    state.projectName = m_session->projectName();
    state.projectDirectory = m_session->projectDirectory();
    if (state.hasProject) {
        state.files = m_session->files();
        state.resultFiles = m_session->resultFiles();
    }
    state.busy = m_busy;
    state.cancellable = m_cancellable;
    state.hasReport = reportExists();
    return state;
}

bool AppController::createProject(const QString &name, const QString &parentDirectory)
{
    m_lastError.clear();

    QString error;
    if (!m_session->createProject(name, parentDirectory, &error)) {
        m_lastError = error;
        emit alertRequested(QStringLiteral("新建项目"), error, QStringLiteral("warning"));
        emit logMessage(error, QStringLiteral("error"));
        return false;
    }

    clearReport();
    setBusy(false, false);
    emit statusMessage(QStringLiteral("已创建项目：%1").arg(m_session->projectName()));
    emitState();
    return true;
}

bool AppController::openProject(const QString &directory)
{
    m_lastError.clear();

    QString error;
    if (!m_session->openProject(directory, &error)) {
        m_lastError = error;
        emit logMessage(error, QStringLiteral("error"));
        return false;
    }

    loadReportFromDisk();
    setBusy(false, false);
    emit logMessage(QStringLiteral("打开项目：%1 (%2)")
                        .arg(m_session->projectName(), m_session->projectDirectory()),
                    QStringLiteral("success"));
    emit statusMessage(QStringLiteral("已打开项目：%1").arg(m_session->projectName()));
    emitState();
    return true;
}

void AppController::closeProject()
{
    if (!m_session->hasProject()) {
        return;
    }
    m_session->closeProject();
    clearReport();
    setBusy(false, false);
    emit statusMessage(QStringLiteral("项目已关闭"));
    emitState();
}

void AppController::importFile(ProjectManager::FileRole role, const QString &sourcePath)
{
    m_lastError.clear();

    if (!m_session->hasProject()) {
        emit alertRequested(QStringLiteral("导入失败"),
                            QStringLiteral("请先新建或打开项目。"), QStringLiteral("warning"));
        return;
    }
    if (m_session->has(role)) {
        emit alertRequested(QStringLiteral("导入限制"),
                            QStringLiteral("每个项目只能有一个%1，请先移除现有文件。")
                                .arg(roleName(role)),
                            QStringLiteral("warning"));
        return;
    }

    QString error;
    const QString projectPath = m_session->copyIntoProject(sourcePath, &error);
    if (projectPath.isEmpty()) {
        m_lastError = error;
        emit alertRequested(QStringLiteral("导入失败"), error, QStringLiteral("warning"));
        emit logMessage(error, QStringLiteral("error"));
        return;
    }

    m_pending = role == ProjectManager::FileRole::Baseline ? Pending::ImportBaseline
                                                           : Pending::ImportEvaluation;
    m_pendingPath = projectPath;
    setBusy(true, false, -1);
    emit statusMessage(QStringLiteral("正在解析 %1 …").arg(QFileInfo(projectPath).fileName()));
    m_importer->start(projectPath);
}

void AppController::evaluate()
{
    if (!m_session->hasProject()
        || !m_session->has(ProjectManager::FileRole::Baseline)
        || !m_session->has(ProjectManager::FileRole::Evaluation)) {
        emit alertRequested(QStringLiteral("数据评估"),
                            QStringLiteral("请先导入基准文件和评估文件。"),
                            QStringLiteral("warning"));
        return;
    }
    if (m_evaluator->isRunning()) {
        return;
    }

    clearReport();

    ev::EvaluationRequest request;
    request.baselinePath = m_session->filePath(ProjectManager::FileRole::Baseline);
    request.evaluationPath = m_session->filePath(ProjectManager::FileRole::Evaluation);
    request.outputDirectory = m_session->projectDirectory();
    request.options.matchTolerance = 0.1;
    request.options.thinInterval = 1.0;

    emit logMessage(QStringLiteral("开始数据评估…"), QStringLiteral("info"));
    setBusy(true, true, 0);
    emit progressChanged(0);
    m_evaluator->start(request);
}

void AppController::cancelEvaluation()
{
    if (m_evaluator->isRunning()) {
        m_evaluator->cancel();
        emit statusMessage(QStringLiteral("正在取消…"));
    }
}

void AppController::removeFile(ProjectManager::FileRole role)
{
    if (!m_session->hasProject()) {
        return;
    }

    QString error;
    if (!m_session->removeFile(role, &error)) {
        emit alertRequested(QStringLiteral("移除文件"), error, QStringLiteral("warning"));
        return;
    }

    clearReport();
    setBusy(false, false);
    emit statusMessage(QStringLiteral("已移除%1").arg(roleName(role)));
    emitState();
}

void AppController::removeResult(const QString &path)
{
    QString error;
    if (!m_session->removeResultFile(path, &error)) {
        emit alertRequested(QStringLiteral("删除结果"), error, QStringLiteral("warning"));
        return;
    }

    if (path.endsWith(QStringLiteral("evaluation_report.html"))
        || path.endsWith(QStringLiteral("evaluation_report.txt"))) {
        clearReport();
    }
    emitState();
}

void AppController::loadTrajectory(const QString &path)
{
    TrajectoryData data;
    if (m_session->cachedTrajectory(path, &data)) {
        emit trajectoryReady(path, data, true);
        return;
    }

    if (m_busy) {
        emit alertRequested(QStringLiteral("请稍候"),
                            QStringLiteral("正在处理其他任务，请等待完成。"),
                            QStringLiteral("info"));
        return;
    }

    m_pending = Pending::Plot;
    m_pendingPath = path;
    setBusy(true, false, -1);
    emit statusMessage(QStringLiteral("正在解析轨迹：%1").arg(QFileInfo(path).fileName()));
    m_importer->start(path);
}

QString AppController::reportHtml()
{
    if (m_reportHtml.isEmpty()) {
        loadReportFromDisk();
    }
    return m_reportHtml;
}

QString AppController::reportText()
{
    if (m_reportText.isEmpty()) {
        loadReportFromDisk();
    }
    return m_reportText;
}

void AppController::onImportFinished(const ImportOutcome &outcome)
{
    const Pending pending = m_pending;
    m_pending = Pending::None;
    setBusy(false, false);

    if (!outcome.ok) {
        m_lastError = outcome.error;
        emit logMessage(outcome.error, QStringLiteral("error"));
        emit alertRequested(QStringLiteral("解析失败"), outcome.error, QStringLiteral("warning"));
        return;
    }

    m_session->cacheTrajectory(outcome.filePath, outcome.data);

    if (pending == Pending::Plot) {
        emit trajectoryReady(outcome.filePath, outcome.data, true);
        emit statusMessage(QStringLiteral("轨迹已就绪"));
        emitState();
        return;
    }

    const auto role = pending == Pending::ImportBaseline
                          ? ProjectManager::FileRole::Baseline
                          : ProjectManager::FileRole::Evaluation;

    QString error;
    if (!m_session->commitImport(role, outcome.filePath, outcome.format, outcome.meta, &error)) {
        emit alertRequested(QStringLiteral("导入失败"), error, QStringLiteral("warning"));
        return;
    }

    emit trajectoryReady(outcome.filePath, outcome.data, false);
    emit statusMessage(QStringLiteral("%1 已导入").arg(roleName(role)));
    emitState();
}

void AppController::onEvaluationFinished(const ev::EvaluationOutcome &outcome)
{
    setBusy(false, false);

    if (!outcome.ok) {
        if (outcome.error == QStringLiteral("已取消")) {
            emit logMessage(QStringLiteral("评估已取消"), QStringLiteral("warning"));
            emit statusMessage(QStringLiteral("评估已取消"));
            return;
        }
        m_lastError = outcome.error;
        emit logMessage(outcome.error, QStringLiteral("error"));
        emit alertRequested(QStringLiteral("数据评估"), outcome.error, QStringLiteral("warning"));
        return;
    }

    m_reportHtml = outcome.reportHtml;
    m_reportText = outcome.reportText;

    QString saveError;
    if (!m_session->setResultFiles(outcome.resultFiles, &saveError)) {
        emit logMessage(saveError, QStringLiteral("warning"));
    }

    const ev::EvaluationResult &result = outcome.result;
    emit logMessage(QStringLiteral("评估完成：匹配 %1 历元，基准固定解 %2，固定-固定 %3")
                        .arg(result.matchedCount)
                        .arg(result.matchedBaselineFix)
                        .arg(result.matchedFixFix),
                    QStringLiteral("success"));
    emit logMessage(QStringLiteral("平面 RMS %1 m / CEP50 %2 m / 95% %3 m")
                        .arg(result.fixFix.rmsPlane, 0, 'f', 4)
                        .arg(result.fixFix.medianPlane, 0, 'f', 4)
                        .arg(result.fixFix.cep95Plane, 0, 'f', 4),
                    QStringLiteral("info"));

    if (!result.valid) {
        emit alertRequested(QStringLiteral("数据评估"),
                            QStringLiteral("评估已完成，但两个文件没有可匹配的历元，请查看报告中的提示。"),
                            QStringLiteral("warning"));
    }

    emit reportReady();
    emitState();
}

void AppController::emitState()
{
    emit stateChanged(state());
}

void AppController::setBusy(bool busy, bool cancellable, int progress)
{
    m_busy = busy;
    m_cancellable = cancellable;
    m_progress = progress;
    emitState();
}

void AppController::clearReport()
{
    m_reportHtml.clear();
    m_reportText.clear();
}

void AppController::loadReportFromDisk()
{
    m_reportHtml.clear();
    m_reportText.clear();
    if (!m_session->hasProject()) {
        return;
    }

    const QDir dir(m_session->projectDirectory());
    readTextFile(dir.filePath(QStringLiteral("evaluation_report.html")), &m_reportHtml);
    readTextFile(dir.filePath(QStringLiteral("evaluation_report.txt")), &m_reportText);
}

bool AppController::reportExists() const
{
    if (!m_reportHtml.isEmpty()) {
        return true;
    }
    if (!m_session->hasProject()) {
        return false;
    }
    const QDir dir(m_session->projectDirectory());
    return QFileInfo::exists(dir.filePath(QStringLiteral("evaluation_report.html")))
           || QFileInfo::exists(dir.filePath(QStringLiteral("evaluation_report.txt")));
}
