#include "projectmanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {

void configureIni(QSettings &settings)
{
#if !defined(QT6)
    settings.setIniCodec("UTF-8");
#else
    Q_UNUSED(settings)
#endif
}

QString roleKey(ProjectManager::FileRole role)
{
    return role == ProjectManager::FileRole::Baseline
               ? QStringLiteral("Baseline")
               : QStringLiteral("Evaluation");
}

} // namespace

QString ProjectManager::configFileName()
{
    return QStringLiteral("project.config");
}

bool ProjectManager::isProjectDirectory(const QString &directory)
{
    return QFileInfo::exists(QDir(directory).filePath(configFileName()));
}

bool ProjectManager::create(const QString &name, const QString &parentDirectory, QString *error)
{
    if (name.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("项目名不能为空");
        return false;
    }

    const QDir parent(parentDirectory);
    if (!parent.exists()) {
        if (error) *error = QStringLiteral("父目录不存在: %1").arg(parentDirectory);
        return false;
    }

    const QString projectDir = parent.filePath(name.trimmed());
    if (!QDir().mkpath(projectDir)) {
        if (error) *error = QStringLiteral("无法创建项目目录: %1").arg(projectDir);
        return false;
    }

    close();
    m_directory = QDir(projectDir).absolutePath();
    m_name = name.trimmed();
    m_created = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    return save(error);
}

bool ProjectManager::open(const QString &directory, QString *error)
{
    if (!isProjectDirectory(directory)) {
        if (error) *error = QStringLiteral("不是项目目录（缺少 %1）: %2").arg(configFileName(), directory);
        return false;
    }

    close();

    QSettings settings(QDir(directory).filePath(configFileName()), QSettings::IniFormat);
    configureIni(settings);

    m_directory = QDir(directory).absolutePath();
    m_name = settings.value(QStringLiteral("Project/Name")).toString();
    m_created = settings.value(QStringLiteral("Project/Created")).toString();

    if (m_name.isEmpty()) {
        m_name = QFileInfo(m_directory).fileName();
    }

    const auto loadRole = [this, &settings](FileRole role, FileRecord *record) {
        const QString key = roleKey(role);
        const QString relative = settings.value(QStringLiteral("Files/%1/Path").arg(key)).toString();
        if (relative.isEmpty()) {
            return;
        }

        const QString absolute = absoluteFromProject(relative);
        if (!QFileInfo::exists(absolute)) {
            return;
        }

        record->role = role;
        record->path = absolute;
        record->format = settings.value(QStringLiteral("Files/%1/Format").arg(key)).toString();
        record->epochCount = settings.value(QStringLiteral("Files/%1/Epochs").arg(key)).toInt();
        record->importedAt = settings.value(QStringLiteral("Files/%1/ImportedAt").arg(key)).toString();
        record->displayName = settings.value(QStringLiteral("Files/%1/DisplayName").arg(key)).toString();
        if (record->displayName.isEmpty()) {
            record->displayName = QFileInfo(absolute).fileName();
        }

        record->meta.format = record->format;
        record->meta.startTime = settings.value(QStringLiteral("Files/%1/Start").arg(key)).toString();
        record->meta.endTime = settings.value(QStringLiteral("Files/%1/End").arg(key)).toString();
        record->meta.intervalSeconds = settings.value(QStringLiteral("Files/%1/Interval").arg(key)).toDouble();
        record->meta.epochCount = record->epochCount;
        record->meta.completenessPercent = settings.value(QStringLiteral("Files/%1/Completeness").arg(key)).toDouble();
        record->meta.fixRatePercent = settings.value(QStringLiteral("Files/%1/FixRate").arg(key)).toDouble();
        record->meta.spanSeconds = settings.value(QStringLiteral("Files/%1/Span").arg(key)).toDouble();
    };

    loadRole(FileRole::Baseline, &m_baseline);
    loadRole(FileRole::Evaluation, &m_evaluation);

    m_results.clear();
    const QStringList stored = settings.value(QStringLiteral("Results/files")).toStringList();
    for (const QString &relative : stored) {
        if (!relative.isEmpty() && QFileInfo::exists(absoluteFromProject(relative))) {
            m_results << relative;
        }
    }

    return true;
}

bool ProjectManager::save(QString *error)
{
    if (!isOpen()) {
        if (error) *error = QStringLiteral("没有打开的项目");
        return false;
    }

    QSettings settings(QDir(m_directory).filePath(configFileName()), QSettings::IniFormat);
    configureIni(settings);

    settings.setValue(QStringLiteral("Project/Name"), m_name);
    settings.setValue(QStringLiteral("Project/Path"), QDir::fromNativeSeparators(m_directory));
    settings.setValue(QStringLiteral("Project/Created"), m_created);
    settings.setValue(QStringLiteral("Project/Version"), 2);

    settings.remove(QStringLiteral("Files"));

    const auto saveRole = [this, &settings](FileRole role, const FileRecord &record) {
        if (record.path.isEmpty()) {
            return;
        }
        const QString key = roleKey(role);
        settings.setValue(QStringLiteral("Files/%1/Path").arg(key), relativeToProject(record.path));
        settings.setValue(QStringLiteral("Files/%1/Format").arg(key), record.format);
        settings.setValue(QStringLiteral("Files/%1/Epochs").arg(key), record.epochCount);
        settings.setValue(QStringLiteral("Files/%1/ImportedAt").arg(key), record.importedAt);
        settings.setValue(QStringLiteral("Files/%1/DisplayName").arg(key), record.displayName);
        settings.setValue(QStringLiteral("Files/%1/Format").arg(key), record.format);
        settings.setValue(QStringLiteral("Files/%1/Start").arg(key), record.meta.startTime);
        settings.setValue(QStringLiteral("Files/%1/End").arg(key), record.meta.endTime);
        settings.setValue(QStringLiteral("Files/%1/Interval").arg(key), record.meta.intervalSeconds);
        settings.setValue(QStringLiteral("Files/%1/Completeness").arg(key), record.meta.completenessPercent);
        settings.setValue(QStringLiteral("Files/%1/FixRate").arg(key), record.meta.fixRatePercent);
        settings.setValue(QStringLiteral("Files/%1/Span").arg(key), record.meta.spanSeconds);
    };

    saveRole(FileRole::Baseline, m_baseline);
    saveRole(FileRole::Evaluation, m_evaluation);

    settings.setValue(QStringLiteral("Results/files"), m_results);
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        if (error) *error = QStringLiteral("写入项目配置失败: %1").arg(m_directory);
        return false;
    }
    return true;
}

void ProjectManager::close()
{
    m_directory.clear();
    m_name.clear();
    m_created.clear();
    m_baseline = FileRecord();
    m_evaluation = FileRecord();
    m_results.clear();
}

void ProjectManager::setFile(const FileRecord &record)
{
    if (record.role == FileRole::Baseline) {
        m_baseline = record;
    } else {
        m_evaluation = record;
    }
}

void ProjectManager::clearFile(FileRole role)
{
    if (role == FileRole::Baseline) {
        m_baseline = FileRecord();
    } else {
        m_evaluation = FileRecord();
    }
}

bool ProjectManager::hasFile(FileRole role) const
{
    return !filePath(role).isEmpty();
}

ProjectManager::FileRecord ProjectManager::file(FileRole role) const
{
    return role == FileRole::Baseline ? m_baseline : m_evaluation;
}

QVector<ProjectManager::FileRecord> ProjectManager::files() const
{
    QVector<FileRecord> records;
    if (!m_baseline.path.isEmpty()) records << m_baseline;
    if (!m_evaluation.path.isEmpty()) records << m_evaluation;
    return records;
}

void ProjectManager::setResultFiles(const QStringList &absolutePaths)
{
    m_results.clear();
    for (const QString &path : absolutePaths) {
        m_results << relativeToProject(path);
    }
}

QStringList ProjectManager::resultFiles() const
{
    QStringList absolute;
    for (const QString &relative : m_results) {
        const QString path = absoluteFromProject(relative);
        if (QFileInfo::exists(path)) {
            absolute << path;
        }
    }
    return absolute;
}

QStringList ProjectManager::resultFileNames() const
{
    QStringList names;
    for (const QString &absolute : resultFiles()) {
        names << QFileInfo(absolute).fileName();
    }
    return names;
}

QString ProjectManager::relativeToProject(const QString &absolutePath) const
{
    if (m_directory.isEmpty()) {
        return absolutePath;
    }
    const QString relative = QDir(m_directory).relativeFilePath(absolutePath);
    return relative.isEmpty() ? absolutePath : relative;
}

QString ProjectManager::absoluteFromProject(const QString &relativePath) const
{
    if (m_directory.isEmpty() || QFileInfo(relativePath).isAbsolute()) {
        return relativePath;
    }
    return QDir(m_directory).absoluteFilePath(relativePath);
}
