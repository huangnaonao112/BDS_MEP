#ifndef EVALUATIONSERVICE_H
#define EVALUATIONSERVICE_H

#include <QAtomicInt>
#include <QFutureWatcher>
#include <QObject>

#include "evaluationpipeline.h"

// 把同步的评估管线搬到后台线程，UI 线程只收进度和结果。
//
// 旧实现在槽函数里同步跑完整个评估：2000 个历元没事，数据量一上来主线程就卡死，
// 进度条和"取消"也无从谈起。
class EvaluationService : public QObject
{
    Q_OBJECT

public:
    explicit EvaluationService(QObject *parent = nullptr);
    ~EvaluationService() override;

    bool isRunning() const;
    void start(const ev::EvaluationRequest &request);
    void cancel();

signals:
    void progressChanged(int percent, const QString &message);
    void messageLogged(const QString &message, const QString &level);
    void finished(const ev::EvaluationOutcome &outcome);

private:
    void ensureWatcher();

    QFutureWatcher<ev::EvaluationOutcome> *m_watcher = nullptr;
    QAtomicInt m_cancelFlag{0};
};

#endif // EVALUATIONSERVICE_H
