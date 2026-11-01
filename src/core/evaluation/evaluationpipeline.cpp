#include "evaluationpipeline.h"

#include "comparisonwriter.h"
#include "gnssparser.h"
#include "reportbuilder.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#if defined(QT6)
#include <QStringConverter>
#endif

namespace ev {

namespace {

bool isCancelled(const ProgressSink &sink)
{
    return sink.cancelled && sink.cancelled();
}

void report(const ProgressSink &sink, int percent, const QString &message)
{
    if (sink.progress) {
        sink.progress(percent, message);
    }
    if (sink.log) {
        sink.log(message, QStringLiteral("info"));
    }
}

void log(const ProgressSink &sink, const QString &message, const QString &level = QStringLiteral("info"))
{
    if (sink.log) {
        sink.log(message, level);
    }
}

bool writeTextFile(const QString &path, const QString &content, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("无法写入 %1: %2").arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
#if defined(QT6)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out << content;
    file.close();
    return true;
}

} // namespace

EvaluationOutcome runEvaluation(const EvaluationRequest &request, const ProgressSink &sink)
{
    EvaluationOutcome outcome;
    QElapsedTimer timer;
    timer.start();

    report(sink, 2, QStringLiteral("解析基准文件：%1").arg(QFileInfo(request.baselinePath).fileName()));

    gnss::ParseResult baseline = gnss::autoParseFile(request.baselinePath);
    if (!baseline.ok) {
        outcome.error = QStringLiteral("基准文件解析失败：%1").arg(baseline.error);
        log(sink, outcome.error, QStringLiteral("error"));
        return outcome;
    }
    for (const QString &warning : baseline.warnings) {
        log(sink, QStringLiteral("基准文件：%1").arg(warning), QStringLiteral("warning"));
    }
    log(sink, QStringLiteral("基准文件：%1 个历元，格式 %2")
                   .arg(baseline.validEpochs)
                   .arg(baseline.series.format),
        QStringLiteral("success"));

    if (isCancelled(sink)) {
        outcome.error = QStringLiteral("已取消");
        return outcome;
    }

    report(sink, 30, QStringLiteral("解析评估文件：%1").arg(QFileInfo(request.evaluationPath).fileName()));

    gnss::ParseResult evaluation = gnss::autoParseFile(request.evaluationPath);
    if (!evaluation.ok) {
        outcome.error = QStringLiteral("评估文件解析失败：%1").arg(evaluation.error);
        log(sink, outcome.error, QStringLiteral("error"));
        return outcome;
    }
    for (const QString &warning : evaluation.warnings) {
        log(sink, QStringLiteral("评估文件：%1").arg(warning), QStringLiteral("warning"));
    }
    log(sink, QStringLiteral("评估文件：%1 个历元，格式 %2")
                   .arg(evaluation.validEpochs)
                   .arg(evaluation.series.format),
        QStringLiteral("success"));

    report(sink, 55, QStringLiteral("时间对齐与精度统计…"));

    if (isCancelled(sink)) {
        outcome.error = QStringLiteral("已取消");
        return outcome;
    }

    ComparisonWriter writer;
    QString writerError;
    if (!writer.open(request.outputDirectory, &writerError)) {
        outcome.error = writerError;
        log(sink, writerError, QStringLiteral("error"));
        return outcome;
    }

    EvaluationResult result = evaluate(baseline.series, evaluation.series, request.options,
                                       [&writer](const ComparisonSample &sample) {
                                           writer.writeSample(sample);
                                       });

    // 文件名与原始路径（报告里要显示用户认识的名字，而不是临时拷贝名）
    result.baseline.fileName = QFileInfo(request.baselinePath).fileName();
    result.evaluation.fileName = QFileInfo(request.evaluationPath).fileName();

    log(sink, QStringLiteral("匹配历元 %1 条，其中基准固定解 %2 条")
                   .arg(result.matchedCount)
                   .arg(result.matchedBaselineFix),
        result.valid ? QStringLiteral("success") : QStringLiteral("warning"));

    for (const QString &warning : result.warnings) {
        log(sink, warning, QStringLiteral("warning"));
    }

    writer.finalize(result);

    if (isCancelled(sink)) {
        outcome.error = QStringLiteral("已取消");
        return outcome;
    }

    report(sink, 80, QStringLiteral("生成评估报告…"));

    const QDateTime generatedAt = QDateTime::currentDateTime();
    outcome.reportText = ReportBuilder::buildText(result, generatedAt);
    outcome.reportHtml = ReportBuilder::buildHtml(result, generatedAt);

    const QDir outputDir(request.outputDirectory);
    const QString textPath = outputDir.filePath(QStringLiteral("evaluation_report.txt"));
    const QString htmlPath = outputDir.filePath(QStringLiteral("evaluation_report.html"));

    QString writeError;
    if (!writeTextFile(textPath, outcome.reportText, &writeError)
        || !writeTextFile(htmlPath, outcome.reportHtml, &writeError)) {
        outcome.error = writeError;
        log(sink, writeError, QStringLiteral("error"));
        return outcome;
    }

    outcome.resultFiles = writer.writtenFiles();
    outcome.resultFiles << textPath << htmlPath;
    result.resultFiles = outcome.resultFiles;

    outcome.result = result;
    outcome.ok = true;
    outcome.elapsedMs = timer.elapsed();

    report(sink, 100, QStringLiteral("评估完成，用时 %1 ms").arg(outcome.elapsedMs, 0, 'f', 0));
    return outcome;
}

} // namespace ev
