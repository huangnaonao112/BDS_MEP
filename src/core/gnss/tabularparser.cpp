#include "gnssparser.h"

#include "gnssdefs.h"
#include "timeutil.h"
#include "textfilereader.h"

#include <QFile>
#include <QTextStream>

#include <cmath>

namespace gnss {

// 表格格式（常见于解算软件导出的 UTC 表）：
//   UTCDate   UTC            GPSTWeek  TOW      Lat        Lon        Height ...
//   2025/07/02 00/00/00.00   2240      0.0      31.9...    118.8...   58.3
// 列顺序按旧版解析器的约定（parts[1]=时刻, parts[4..6]=BLH），另外兼容
// 首列带日期的写法。
ParseResult parseTabularFile(const QString &path)
{
    ParseResult result;

    QFile file;
    QTextStream in;
    QString error;
    if (!TextFileReader::open(path, file, in, &result.encoding, &error)) {
        result.error = error;
        return result;
    }

    result.series.format = QStringLiteral("TABULAR");
    result.series.sourceFile = path;

    QVector<char> dated;
    dated.reserve(4096);

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        ++result.totalLines;

        // 前两行是表头
        if (result.totalLines <= 2 || line.isEmpty() || line.startsWith(QLatin1Char('('))
            || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 7) {
            ++result.skippedLines;
            continue;
        }

        double dayIndex = 0.0;
        const bool hasDate = parseDateDayIndex(parts[0], &dayIndex);

        double timeOfDay = 0.0;
        if (!parseClock(parts[1], &timeOfDay)) {
            // 旧格式里时刻写成 HH/MM/SS.FF
            const QStringList hms = parts[1].split(QLatin1Char('/'));
            if (hms.size() == 3) {
                bool okH = false, okM = false, okS = false;
                const int hh = hms[0].toInt(&okH);
                const int mm = hms[1].toInt(&okM);
                const double ss = hms[2].toDouble(&okS);
                if (!okH || !okM || !okS) {
                    ++result.skippedLines;
                    continue;
                }
                timeOfDay = hh * 3600.0 + mm * 60.0 + ss;
            } else {
                // 退化：用 GPST 周/周内秒折算当天 UTC 时刻（扣 18 s 跳秒）
                bool okWeek = false, okTow = false;
                const int week = parts[2].toInt(&okWeek);
                const double tow = parts[3].toDouble(&okTow);
                if (!okWeek || !okTow) {
                    ++result.skippedLines;
                    continue;
                }
                timeOfDay = wrapToDay(gpsToUnixSeconds(week, tow) - kLeapSeconds);
            }
        }

        bool ok = false;
        const double lat = parts[4].toDouble(&ok);
        if (!ok) { ++result.skippedLines; continue; }
        const double lon = parts[5].toDouble(&ok);
        if (!ok) { ++result.skippedLines; continue; }
        const double height = parts[6].toDouble(&ok);
        if (!ok) { ++result.skippedLines; continue; }

        if (!isFinite(Blh{lat, lon, height}) || std::fabs(lat) > 90.0 || std::fabs(lon) > 180.0) {
            ++result.skippedLines;
            continue;
        }

        GnssEpoch epoch;
        epoch.time = hasDate ? (dayIndexToUnixSeconds(dayIndex) + timeOfDay) : timeOfDay;
        epoch.lat = lat;
        epoch.lon = lon;
        epoch.height = height;
        epoch.hasBlh = true;
        // 表格源没有解状态字段，按"基准/固定"处理（报告中会注明该口径）
        epoch.quality = 1;

        result.series.epochs.append(epoch);
        dated.append(hasDate ? 1 : 0);
    }

    file.close();

    result.validEpochs = result.series.epochs.size();
    if (result.validEpochs == 0) {
        result.error = QStringLiteral("未找到有效的表格数据行（共 %1 行）").arg(result.totalLines);
        return result;
    }

    int datedCount = 0;
    for (char flag : dated) {
        datedCount += flag ? 1 : 0;
    }

    if (datedCount == result.validEpochs) {
        result.series.hasDate = true;
    } else {
        result.series.hasDate = false;
        if (datedCount > 0) {
            for (int i = 0; i < result.series.epochs.size(); ++i) {
                if (dated[i]) {
                    result.series.epochs[i].time = wrapToDay(result.series.epochs[i].time);
                }
            }
        }
        result.warnings << QStringLiteral("表格文件没有日期列，按当天秒对齐");
    }

    result.warnings << QStringLiteral("表格源不含解状态字段，全部历元按固定解参与统计");

    result.ok = true;
    return result;
}

} // namespace gnss
