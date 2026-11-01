#ifndef IMPORTSERVICE_H
#define IMPORTSERVICE_H

#include <QAtomicInt>
#include <QFutureWatcher>
#include <QObject>

#include "fileparser.h"

struct ImportOutcome {
    bool ok = false;
    QString filePath;      // 实际解析的路径（项目内副本）
    QString sourcePath;    // 用户选择的原始路径
    QString format;        // GGA / POS / TABULAR
    QString error;
    QStringList warnings;
    TrajectoryData data;
    TrackMeta meta;
    double elapsedMs = 0.0;
};

// 文件导入解析的后台服务：解析 20 万行的 .pos 不再冻结界面。
class ImportService : public QObject
{
    Q_OBJECT

public:
    explicit ImportService(QObject *parent = nullptr);
    ~ImportService() override;

    bool isRunning() const;
    void start(const QString &filePath);

signals:
    void messageLogged(const QString &message, const QString &level);
    void finished(const ImportOutcome &outcome);

private:
    QFutureWatcher<ImportOutcome> *m_watcher = nullptr;
};

#endif // IMPORTSERVICE_H
