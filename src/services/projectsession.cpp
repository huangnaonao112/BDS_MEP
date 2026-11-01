#include "projectsession.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

QString roleLabel(ProjectManager::FileRole role)
{
    return role == ProjectManager::FileRole::Baseline ? QStringLiteral("基准")
                                                      : QStringLiteral("评估");
}

} // namespace

ProjectSession::ProjectSession(QObject *parent)
    : QObject(parent)
{
}

bool ProjectSession::createProject(const QString &name, const QString &parentDirectory, QString *error)
{
    if (!m_project.create(name, parentDirectory, error)) {
        return false;
    }
    m_cache.clear();
    emit projectOpened(m_project.name(), m_project.directory());
    emit filesChanged();
    emit resultsChanged();
    return true;
}

bool ProjectSession::openProject(const QString &directory, QString *error)
{
    ProjectManager candidate;
    if (!candidate.open(directory, error)) {
        return false;
    }

    m_project = candidate;
    m_cache.clear();

    emit projectOpened(m_project.name(), m_project.directory());
    emit filesChanged();
    emit resultsChanged();
    return true;
}

void ProjectSession::closeProject()
{
    m_project.close();
    m_cache.clear();
    emit projectClosed();
    emit filesChanged();
    emit resultsChanged();
}

QString ProjectSession::copyIntoProject(const QString &sourcePath, QString *error)
{
    if (!m_project.isOpen()) {
        if (error) *error = QStringLiteral("尚未打开项目");
        return QString();
    }

    const QFileInfo source(sourcePath);
    if (!source.exists() || !source.isFile()) {
        if (error) *error = QStringLiteral("文件不存在: %1").arg(sourcePath);
        return QString();
    }

    const QDir projectDir(m_project.directory());
    const QString sameDir = QFileInfo(source.absolutePath()).absoluteFilePath();
    if (sameDir.compare(projectDir.absolutePath(), Qt::CaseInsensitive) == 0) {
        return source.absoluteFilePath();
    }

    // 同名文件不覆盖：加序号后缀
    QString targetName = source.fileName();
    QString targetPath = projectDir.filePath(targetName);
    int suffix = 1;
    while (QFile::exists(targetPath)) {
        const QString stem = source.completeBaseName();
        targetName = QStringLiteral("%1_%2.%3").arg(stem).arg(suffix).arg(source.suffix());
        targetPath = projectDir.filePath(targetName);
        ++suffix;
    }

    if (!QFile::copy(source.absoluteFilePath(), targetPath)) {
        if (error) *error = QStringLiteral("复制文件到项目目录失败: %1").arg(targetPath);
        return QString();
    }
    return targetPath;
}

bool ProjectSession::commitImport(ProjectManager::FileRole role, const QString &projectPath,
                                  const QString &format, const TrackMeta &meta, QString *error)
{
    if (!m_project.isOpen()) {
        if (error) *error = QStringLiteral("尚未打开项目");
        return false;
    }

    ProjectManager::FileRecord record;
    record.role = role;
    record.path = projectPath;
    record.format = format;
    record.epochCount = meta.epochCount;
    record.displayName = QStringLiteral("%1 (%2%3)")
                             .arg(QFileInfo(projectPath).fileName(), format, roleLabel(role));
    record.importedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    record.meta = meta;
    record.meta.format = format;

    m_project.setFile(record);
    if (!m_project.save(error)) {
        return false;
    }

    emit filesChanged();
    return true;
}

bool ProjectSession::removeFile(ProjectManager::FileRole role, QString *error)
{
    const QString path = m_project.filePath(role);
    if (!path.isEmpty()) {
        m_cache.remove(path);
        m_project.clearFile(role);
    }

    // 删掉文件后旧的评估结果已经失效，一并清掉
    m_project.setResultFiles(QStringList());
    if (!m_project.save(error)) {
        return false;
    }

    emit filesChanged();
    emit resultsChanged();
    return true;
}

bool ProjectSession::setResultFiles(const QStringList &paths, QString *error)
{
    m_project.setResultFiles(paths);
    if (!m_project.save(error)) {
        return false;
    }
    emit resultsChanged();
    return true;
}

bool ProjectSession::removeResultFile(const QString &path, QString *error)
{
    QStringList remaining;
    for (const QString &existing : m_project.resultFiles()) {
        if (QFileInfo(existing).absoluteFilePath().compare(QFileInfo(path).absoluteFilePath()) != 0) {
            remaining << existing;
        }
    }

    m_project.setResultFiles(remaining);
    if (!m_project.save(error)) {
        return false;
    }
    emit resultsChanged();
    return true;
}

void ProjectSession::cacheTrajectory(const QString &path, const TrajectoryData &data)
{
    m_cache.insert(path, data);
}

bool ProjectSession::cachedTrajectory(const QString &path, TrajectoryData *data) const
{
    const auto it = m_cache.constFind(path);
    if (it == m_cache.constEnd()) {
        return false;
    }
    if (data) {
        *data = it.value();
    }
    return true;
}

void ProjectSession::clearCache()
{
    m_cache.clear();
}
