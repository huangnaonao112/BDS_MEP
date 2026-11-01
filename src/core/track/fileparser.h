#ifndef FILEPARSER_H
#define FILEPARSER_H

#include <QString>
#include <QStringList>
#include <functional>

#include "trajectorydata.h"

// 面向 UI 的解析门面：调用 gnss 解析器、规范化时间轴、构建 ENU 轨迹。
//
// 旧版这里另有一整套 parseGgaFile / parsePosFile / gpst2time，
// 与 DataEvaluator 的实现互为副本。现在全部收敛到 gnss/ 下的一份实现。
class FileParser
{
public:
    using Logger = std::function<void(const QString &message, const QString &level)>;

    struct Result {
        bool ok = false;
        QString format;
        QString error;
        QStringList warnings;
        TrajectoryData data;
        int epochs = 0;
        double elapsedMs = 0.0;
    };

    static Result parse(const QString &filePath, Logger logger = nullptr);

private:
    static void log(Logger logger, const QString &message, const QString &level = QStringLiteral("info"));
};

#endif // FILEPARSER_H
