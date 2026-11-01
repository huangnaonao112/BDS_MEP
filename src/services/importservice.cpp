#include "importservice.h"

#include <QElapsedTimer>
#include <QFuture>
#include <QMetaObject>
#include <QtConcurrent>

ImportService::ImportService(QObject *parent)
    : QObject(parent)
{
}

ImportService::~ImportService()
{
    if (m_watcher) {
        m_watcher->waitForFinished();
    }
}

bool ImportService::isRunning() const
{
    return m_watcher && m_watcher->isRunning();
}

void ImportService::start(const QString &filePath)
{
    if (isRunning()) {
        return;
    }

    if (!m_watcher) {
        m_watcher = new QFutureWatcher<ImportOutcome>(this);
        connect(m_watcher, &QFutureWatcher<ImportOutcome>::finished, this, [this]() {
            emit finished(m_watcher->result());
        });
    }

    const QFuture<ImportOutcome> future = QtConcurrent::run([this, filePath]() {
        ImportOutcome outcome;
        outcome.sourcePath = filePath;
        outcome.filePath = filePath;

        QElapsedTimer timer;
        timer.start();

        const FileParser::Result parsed = FileParser::parse(filePath, [this](const QString &message, const QString &level) {
            QMetaObject::invokeMethod(this, [this, message, level]() {
                emit messageLogged(message, level);
            }, Qt::QueuedConnection);
        });

        outcome.ok = parsed.ok;
        outcome.error = parsed.error;
        outcome.format = parsed.format;
        outcome.warnings = parsed.warnings;
        outcome.data = parsed.data;
        outcome.meta = parsed.data.meta();
        outcome.meta.format = parsed.format;
        outcome.elapsedMs = timer.elapsed();
        return outcome;
    });

    m_watcher->setFuture(future);
}
