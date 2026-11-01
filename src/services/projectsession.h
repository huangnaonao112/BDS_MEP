#ifndef PROJECTSESSION_H
#define PROJECTSESSION_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "projectmanager.h"
#include "trajectorydata.h"

// 会话层：把"项目状态"从 MainWindow 里抽出来。
//
// 旧 MainWindow 749 行，既管界面、又管缓存、又管文件复制和项目配置，
// 谁都改不动。现在它只说"用户点了什么"，状态在这里。
class ProjectSession : public QObject
{
    Q_OBJECT

public:
    explicit ProjectSession(QObject *parent = nullptr);

    bool createProject(const QString &name, const QString &parentDirectory, QString *error = nullptr);
    bool openProject(const QString &directory, QString *error = nullptr);
    void closeProject();

    bool hasProject() const { return m_project.isOpen(); }
    QString projectName() const { return m_project.name(); }
    QString projectDirectory() const { return m_project.directory(); }

    // 把用户选的文件复制进项目目录（已在项目目录内则原样返回）
    QString copyIntoProject(const QString &sourcePath, QString *error = nullptr);

    bool commitImport(ProjectManager::FileRole role, const QString &projectPath,
                      const QString &format, const TrackMeta &meta, QString *error = nullptr);
    bool removeFile(ProjectManager::FileRole role, QString *error = nullptr);

    bool has(ProjectManager::FileRole role) const { return m_project.hasFile(role); }
    QString filePath(ProjectManager::FileRole role) const { return m_project.filePath(role); }
    QString displayName(ProjectManager::FileRole role) const { return m_project.file(role).displayName; }
    QVector<ProjectManager::FileRecord> files() const { return m_project.files(); }

    bool setResultFiles(const QStringList &paths, QString *error = nullptr);
    bool removeResultFile(const QString &path, QString *error = nullptr);
    QStringList resultFiles() const { return m_project.resultFiles(); }
    QStringList resultFileNames() const { return m_project.resultFileNames(); }

    void cacheTrajectory(const QString &path, const TrajectoryData &data);
    bool cachedTrajectory(const QString &path, TrajectoryData *data) const;
    void clearCache();

signals:
    void projectOpened(const QString &name, const QString &directory);
    void projectClosed();
    void filesChanged();
    void resultsChanged();

private:
    ProjectManager m_project;
    QHash<QString, TrajectoryData> m_cache;
};

#endif // PROJECTSESSION_H
