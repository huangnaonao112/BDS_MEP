#ifndef GNSS_GNSSPARSER_H
#define GNSS_GNSSPARSER_H

#include <QString>
#include <QStringList>

#include "epochdata.h"

// 文件解析统一入口。
//
// 旧实现里 parseGgaFile / parsePosFile / parseTabularFile 在 DataEvaluator 与
// FileParser 中各写了一遍（还有第二份 gpst2time、第二份质量归一化）。现在只有
// 这里一份，UI 与评估引擎共用。
namespace gnss {

struct ParseResult {
    EpochSeries series;
    bool ok = false;
    QString error;
    QString encoding;
    QStringList warnings;

    int totalLines = 0;
    int validEpochs = 0;
    int skippedLines = 0;
};

enum class FileFormat {
    Unknown,
    Gga,
    Pos,
    Tabular
};

QString formatName(FileFormat format);
FileFormat detectFormat(const QString &path);

ParseResult parseGgaFile(const QString &path);
ParseResult parsePosFile(const QString &path);
ParseResult parseTabularFile(const QString &path);

// 按内容/扩展名选择解析器；首选失败时按优先级回退
ParseResult autoParseFile(const QString &path);

// 解析器只做"读文件 -> 历元序列"，不做抽稀、不做 ENU 转换。
// 这两个动作分别由 thinSeries() 和评估引擎完成。

} // namespace gnss

#endif // GNSS_GNSSPARSER_H
