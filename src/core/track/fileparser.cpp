#include "fileparser.h"

#include "gnssparser.h"
#include "epochdata.h"

#include <QElapsedTimer>
#include <QFileInfo>

void FileParser::log(Logger logger, const QString &message, const QString &level)
{
    if (logger) {
        logger(message, level);
    }
}

FileParser::Result FileParser::parse(const QString &filePath, Logger logger)
{
    Result result;
    QElapsedTimer timer;
    timer.start();

    const QFileInfo info(filePath);
    log(logger, QStringLiteral("解析文件: %1").arg(info.fileName()));

    gnss::ParseResult parsed = gnss::autoParseFile(filePath);
    if (!parsed.ok) {
        result.error = parsed.error.isEmpty()
                           ? QStringLiteral("解析失败: %1").arg(filePath)
                           : parsed.error;
        result.elapsedMs = timer.elapsed();
        log(logger, result.error, QStringLiteral("error"));
        return result;
    }

    result.format = parsed.series.format;

    if (!parsed.encoding.isEmpty()) {
        log(logger, QStringLiteral("文本编码: %1").arg(parsed.encoding));
    }
    log(logger, QStringLiteral("识别格式: %1，有效历元 %2 条（跳过 %3 行）")
                   .arg(parsed.series.format)
                   .arg(parsed.validEpochs)
                   .arg(parsed.skippedLines));

    for (const QString &warning : parsed.warnings) {
        log(logger, warning, QStringLiteral("warning"));
    }

    const QStringList normalizeNotes = gnss::normalizeSeries(parsed.series);
    for (const QString &note : normalizeNotes) {
        log(logger, note, QStringLiteral("warning"));
    }

    QString buildError;
    if (!result.data.buildFromSeries(parsed.series, &buildError)) {
        result.error = buildError;
        result.elapsedMs = timer.elapsed();
        log(logger, result.error, QStringLiteral("error"));
        return result;
    }

    result.ok = true;
    result.epochs = result.data.getPointCount();
    result.elapsedMs = timer.elapsed();

    const TrackMeta meta = result.data.meta();
    log(logger, QStringLiteral("解析完成：%1 历元，%2 - %3，采样间隔 %4 s，固定率 %5%")
                   .arg(meta.epochCount)
                   .arg(meta.startTime, meta.endTime)
                   .arg(QString::number(meta.intervalSeconds, 'f', 2))
                   .arg(QString::number(meta.fixRatePercent, 'f', 1)),
        QStringLiteral("success"));

    return result;
}
