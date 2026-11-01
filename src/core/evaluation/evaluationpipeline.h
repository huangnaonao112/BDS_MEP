#ifndef EVALUATIONPIPELINE_H
#define EVALUATIONPIPELINE_H

#include <QString>
#include <QStringList>
#include <functional>

#include "evaluationengine.h"
#include "evaluationresult.h"

// 一次完整评估：解析 -> 对齐统计 -> 写明细 -> 生成报告。
//
// 纯同步实现，不依赖 UI 线程；UI 侧用 EvaluationService 把它放到后台线程。
namespace ev {

struct EvaluationRequest {
    QString baselinePath;
    QString evaluationPath;
    QString outputDirectory;
    EvaluationOptions options;
};

struct ProgressSink {
    std::function<void(int percent, const QString &message)> progress;
    std::function<void(const QString &message, const QString &level)> log;
    std::function<bool()> cancelled;
};

struct EvaluationOutcome {
    bool ok = false;
    QString error;
    EvaluationResult result;
    QString reportText;
    QString reportHtml;
    QStringList resultFiles;
    double elapsedMs = 0.0;
};

EvaluationOutcome runEvaluation(const EvaluationRequest &request, const ProgressSink &sink);

} // namespace ev

#endif // EVALUATIONPIPELINE_H
