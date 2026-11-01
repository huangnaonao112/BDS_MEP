#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "trackmeta.h"

// 项目持久化。
//
// 旧版 project.config 只写了 Name/Path，重启后任务树、文件列表、结果文件全丢，
// 所谓"新建项目"其实只是一个文件夹。这里把基准文件、评估文件、结果清单都写进
// 配置（相对路径），Open 时校验并重建，才算真正的"打开项目"。
class ProjectManager
{
public:
    enum class FileRole {
        Baseline,
        Evaluation
    };

    struct FileRecord {
        FileRole role = FileRole::Baseline;
        QString path;           // 绝对路径
        QString displayName;    // 任务树显示名
        QString format;         // GGA / POS / TABULAR
        int epochCount = 0;
        QString importedAt;
        TrackMeta meta;         // 打开项目时无需重新解析即可还原表格
    };

    static QString configFileName();
    static bool isProjectDirectory(const QString &directory);

    bool create(const QString &name, const QString &parentDirectory, QString *error = nullptr);
    bool open(const QString &directory, QString *error = nullptr);
    bool save(QString *error = nullptr);
    void close();

    bool isOpen() const { return !m_directory.isEmpty(); }

    QString directory() const { return m_directory; }
    QString name() const { return m_name; }

    void setFile(const FileRecord &record);
    void clearFile(FileRole role);
    bool hasFile(FileRole role) const;
    FileRecord file(FileRole role) const;
    QString filePath(FileRole role) const { return file(role).path; }
    QVector<FileRecord> files() const;

    // 结果文件相对项目目录保存，换机器/换盘符仍可用
    void setResultFiles(const QStringList &absolutePaths);
    QStringList resultFiles() const;          // 绝对路径且存在的
    QStringList resultFileNames() const;      // 文件名

    QString relativeToProject(const QString &absolutePath) const;
    QString absoluteFromProject(const QString &relativePath) const;

private:
    QString m_directory;
    QString m_name;
    QString m_created;
    FileRecord m_baseline;
    FileRecord m_evaluation;
    QStringList m_results;    // 相对路径
};

#endif // PROJECTMANAGER_H
