#include "gnssparser.h"

#include "gnssdefs.h"
#include "timeutil.h"
#include "textfilereader.h"

#include <QFile>
#include <QTextStream>

#include <cmath>

namespace gnss {

namespace {

inline bool isGgaSentence(const QString &line)
{
    // $GPGGA / $GNGGA / $BDGGA / $GAGGA ...：语句类型以 GGA 结尾
    if (line.size() < 7 || line.at(0) != QLatin1Char('$')) {
        return false;
    }
    const int comma = line.indexOf(QLatin1Char(','));
    if (comma < 5) {
        return false;
    }
    return line.mid(1, comma - 1).endsWith(QStringLiteral("GGA"));
}

} // namespace

ParseResult parseGgaFile(const QString &path)
{
    ParseResult result;

    QFile file;
    QTextStream in;
    QString error;
    if (!TextFileReader::open(path, file, in, &result.encoding, &error)) {
        result.error = error;
        return result;
    }

    result.series.format = QStringLiteral("GGA");
    result.series.sourceFile = path;
    result.series.hasDate = false;  // NMEA GGA 本身不带日期

    while (!in.atEnd()) {
        const QString rawLine = in.readLine().trimmed();
        ++result.totalLines;

        if (rawLine.isEmpty()) {
            continue;
        }
        if (!isGgaSentence(rawLine)) {
            if (rawLine.startsWith(QLatin1Char('$'))) {
                ++result.skippedLines;  // 其它 NMEA 语句
            }
            continue;
        }

        // 去掉校验和
        QString line = rawLine;
        const int star = line.indexOf(QLatin1Char('*'));
        if (star >= 0) {
            line = line.left(star);
        }

        const QStringList parts = line.split(QLatin1Char(','));
        // GGA 至少到第 10 个字段（海拔 + 单位）；允许尾部字段缺失
        if (parts.size() < 10) {
            ++result.skippedLines;
            continue;
        }

        const QString &timeText = parts[1];
        const QString &latText = parts[2];
        const QString &latDir = parts[3];
        const QString &lonText = parts[4];
        const QString &lonDir = parts[5];
        const QString &qualityText = parts[6];
        const QString &satText = parts[7];
        const QString &hdopText = parts[8];
        const QString &altText = parts[9];

        if (latText.isEmpty() || lonText.isEmpty()) {
            ++result.skippedLines;
            continue;
        }

        double timeOfDay = 0.0;
        if (!parseNmeaTimeOfDay(timeText, &timeOfDay)) {
            ++result.skippedLines;
            continue;
        }

        bool ok = false;
        const double rawLat = latText.toDouble(&ok);
        if (!ok || rawLat < 0.0) {
            ++result.skippedLines;
            continue;
        }
        const double rawLon = lonText.toDouble(&ok);
        if (!ok || rawLon < 0.0) {
            ++result.skippedLines;
            continue;
        }

        int ggaQuality = 0;
        if (!qualityText.isEmpty()) {
            ggaQuality = qualityText.toInt(&ok);
            if (!ok) {
                ++result.skippedLines;
                continue;
            }
        }

        const int quality = normalizeNmeaQuality(ggaQuality);
        if (!isValid(quality)) {
            // 0=无定位；这类历元没有可用位置，不计入有效数据
            ++result.skippedLines;
            continue;
        }

        // NMEA: ddmm.mmmm / dddmm.mmmm -> 十进制度
        const double latDeg = std::floor(rawLat / 100.0);
        const double latMin = rawLat - latDeg * 100.0;
        double latitude = latDeg + latMin / 60.0;

        const double lonDeg = std::floor(rawLon / 100.0);
        const double lonMin = rawLon - lonDeg * 100.0;
        double longitude = lonDeg + lonMin / 60.0;

        if (latMin >= 60.0 || lonMin >= 60.0) {
            ++result.skippedLines;
            continue;
        }
        if (latDir == QLatin1String("S")) latitude = -latitude;
        if (lonDir == QLatin1String("W")) longitude = -longitude;

        const double altitude = altText.toDouble(&ok);
        if (!ok) {
            ++result.skippedLines;
            continue;
        }

        GnssEpoch epoch;
        epoch.time = timeOfDay;
        epoch.lat = latitude;
        epoch.lon = longitude;
        epoch.height = altitude;
        epoch.hasBlh = true;
        epoch.quality = quality;
        epoch.satellites = satText.isEmpty() ? 0 : satText.toInt();
        epoch.hdop = hdopText.isEmpty() ? 0.0 : hdopText.toDouble();

        if (!isFinite(Blh{epoch.lat, epoch.lon, epoch.height})) {
            ++result.skippedLines;
            continue;
        }

        result.series.epochs.append(epoch);
    }

    file.close();

    result.validEpochs = result.series.epochs.size();
    result.ok = result.validEpochs > 0;
    if (!result.ok) {
        result.error = QStringLiteral("未找到有效的 GGA 定位语句（共 %1 行）").arg(result.totalLines);
    }
    return result;
}

} // namespace gnss
