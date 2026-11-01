#include "evaluationservice.h"

#include <QFuture>
#include <QMetaObject>
#include <QtConcurrent>

EvaluationService::EvaluationService(QObject *parent)
    : QObject(parent)
{
}

EvaluationService::~EvaluationService()
{
    cancel();
    if (m_watcher) {
        m_watcher->waitForFinished();
    }
}

bool EvaluationService::isRunning() const
{
    return m_watcher && m_watcher->isRunning();
}

void EvaluationService::ensureWatcher()
{
    if (m_watcher) {
        return;
    }

    m_watcher = new QFutureWatcher<ev::EvaluationOutcome>(this);
    connect(m_watcher, &QFutureWatcher<ev::EvaluationOutcome>::finished, this, [this]() {
        const ev::EvaluationOutcome outcome = m_watcher->result();
        emit finished(outcome);
    });
}

void EvaluationService::start(const ev::EvaluationRequest &request)
{
    if (isRunning()) {
        return;
    }

    ensureWatcher();
    m_cancelFlag.storeRelease(0);

    const QFuture<ev::EvaluationOutcome> future = QtConcurrent::run([this, request]() {
        ev::ProgressSink sink;
        sink.progress = [this](int percent, const QString &message) {
            QMetaObject::invokeMethod(this, [this, percent, message]() {
                emit progressChanged(percent, message);
            }, Qt::QueuedConnection);
        };
        sink.log = [this](const QString &message, const QString &level) {
            QMetaObject::invokeMethod(this, [this, message, level]() {
                emit messageLogged(message, level);
            }, Qt::QueuedConnection);
        };
        sink.cancelled = [this]() { return m_cancelFlag.loadAcquire() != 0; };
        return ev::runEvaluation(request, sink);
    });

    m_watcher->setFuture(future);
}

void EvaluationService::cancel()
{
    if (!isRunning()) {
        return;
    }
    m_cancelFlag.storeRelease(1);
}
