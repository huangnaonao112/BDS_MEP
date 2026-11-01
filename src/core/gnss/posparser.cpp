#include "gnssparser.h"

#include "gnssdefs.h"
#include "timeutil.h"
#include "textfilereader.h"

#include <QFile>
#include <QTextStream>

#include <cmath>

namespace gnss {

namespace {

// RTKLIB .pos 数据行有两种写法：
//   A) date time x y z Q ns ...   （标准 pos/stat）
//   B) time x y z Q ns ...        （省略日期的工具输出）
// 用"第一列是不是日期"区分，列偏移由 idx 决定，而不是写死 parts[2]。
int detectColumnOffset(const QStringList &parts, bool *hasDate, double *seconds)
{
    if (parts.size() < 12) {
        return -1;
    }

    double absolute = 0.0;
    if (parseDateTime(parts[0], parts[1], &absolute)) {
        *hasDate = true;
        *seconds = absolute;
        return 2;
    }

    double timeOfDay = 0.0;
    if (parseClock(parts[0], &timeOfDay)) {
        *hasDate = false;
        *seconds = timeOfDay;
        return 1;
    }

    return -1;
}

} // namespace

ParseResult parsePosFile(const QString &path)
{
    ParseResult result;

    QFile file;
    QTextStream in;
    QString error;
    if (!TextFileReader::open(path, file, in, &result.encoding, &error)) {
        result.error = error;
        return result;
    }

    result.series.format = QStringLiteral("POS");
    result.series.sourceFile = path;

    QVector<char> dated;
    dated.reserve(4096);

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        ++result.totalLines;

        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1Char('%')) || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        bool hasDate = false;
        double seconds = 0.0;
        const int idx = detectColumnOffset(parts, &hasDate, &seconds);
        if (idx < 0 || parts.size() < idx + 11) {
            ++result.skippedLines;
            continue;
        }

        bool ok = false;
        const double x = parts[idx].toDouble(&ok);
        if (!ok) { ++result.skippedLines; continue; }
        const double y = parts[idx + 1].toDouble(&ok);
        if (!ok) { ++result.skippedLines; continue; }
        const double z = parts[idx + 2].toDouble(&ok);
        if (!ok) { ++result.skippedLines; continue; }

        const int quality = parts[idx + 3].toInt(&ok);
        if (!ok || !isValid(quality)) {
            ++result.skippedLines;
            continue;
        }

        if (!isFinite(Ecef{x, y, z})) {
            ++result.skippedLines;
            continue;
        }

        GnssEpoch epoch;
        epoch.time = seconds;
        epoch.x = x;
        epoch.y = y;
        epoch.z = z;
        epoch.hasEcef = true;
        epoch.quality = quality;
        epoch.satellites = parts[idx + 4].toInt();
        epoch.sdx = parts[idx + 5].toDouble();
        epoch.sdy = parts[idx + 6].toDouble();
        epoch.sdz = parts[idx + 7].toDouble();
        if (parts.size() > idx + 8) epoch.sdxy = parts[idx + 8].toDouble();
        if (parts.size() > idx + 9) epoch.sdyz = parts[idx + 9].toDouble();
        if (parts.size() > idx + 10) epoch.sdzx = parts[idx + 10].toDouble();

        // 渲染层用 hdop 画误差圆，沿用旧口径：平面 1σ 估计（单位 m）
        epoch.hdop = std::sqrt(epoch.sdx * epoch.sdx + epoch.sdy * epoch.sdy);

        result.series.epochs.append(epoch);
        dated.append(hasDate ? 1 : 0);
    }

    file.close();

    result.validEpochs = result.series.epochs.size();
    if (result.validEpochs == 0) {
        result.error = QStringLiteral("未找到有效的 POS/STAT 数据行（共 %1 行）").arg(result.totalLines);
        return result;
    }

    int datedCount = 0;
    for (char flag : dated) {
        datedCount += flag ? 1 : 0;
    }

    if (datedCount == result.validEpochs) {
        result.series.hasDate = true;
    } else if (datedCount == 0) {
        result.series.hasDate = false;
        result.warnings << QStringLiteral("POS 文件缺少日期列，已按当天秒（时刻）处理");
    } else {
        result.series.hasDate = false;
        result.warnings << QStringLiteral("%1/%2 个历元缺少日期，已统一退化为当天秒对齐")
                               .arg(result.validEpochs - datedCount)
                               .arg(result.validEpochs);
        for (int i = 0; i < result.series.epochs.size(); ++i) {
            if (dated[i]) {
                result.series.epochs[i].time = wrapToDay(result.series.epochs[i].time);
            }
        }
    }

    result.ok = true;
    return result;
}

} // namespace gnss
