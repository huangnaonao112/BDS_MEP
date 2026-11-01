#include "gnssparser.h"

#include "timeutil.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace gnss {

namespace {

// 用开头若干行判断格式，避免把 200 MB 的文件全读进来
QStringList probeLines(const QString &path, int maxLines)
{
    QStringList lines;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return lines;
    }
    while (!file.atEnd() && lines.size() < maxLines) {
        const QByteArray raw = file.readLine(4096);
        if (raw.isEmpty()) {
            break;
        }
        lines << QString::fromLatin1(raw).trimmed();
    }
    return lines;
}

} // namespace

QString formatName(FileFormat format)
{
    switch (format) {
    case FileFormat::Gga: return QStringLiteral("GGA");
    case FileFormat::Pos: return QStringLiteral("POS");
    case FileFormat::Tabular: return QStringLiteral("TABULAR");
    default: return QStringLiteral("UNKNOWN");
    }
}

FileFormat detectFormat(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("pos") || suffix == QLatin1String("stat")
        || suffix == QLatin1String("ubx")) {
        return FileFormat::Pos;
    }
    if (suffix == QLatin1String("nmea") || suffix == QLatin1String("gga")) {
        return FileFormat::Gga;
    }

    const QStringList lines = probeLines(path, 12);
    bool sawGga = false;
    bool sawPosDate = false;
    bool sawTabularHeader = false;

    for (const QString &line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1Char('$'))) {
            const int comma = line.indexOf(QLatin1Char(','));
            if (comma > 0 && line.mid(1, comma - 1).endsWith(QStringLiteral("GGA"))) {
                sawGga = true;
            }
            continue;
        }
        if (line.contains(QStringLiteral("UTCDate"), Qt::CaseInsensitive)) {
            sawTabularHeader = true;
            continue;
        }
        if (line.startsWith(QLatin1Char('%')) || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }
        double dayIndex = 0.0;
        if (parseDateDayIndex(parts[0], &dayIndex)) {
            sawPosDate = true;
            break;
        }
    }

    if (sawGga) return FileFormat::Gga;
    if (sawTabularHeader) return FileFormat::Tabular;
    if (sawPosDate) return FileFormat::Pos;
    return FileFormat::Unknown;
}

ParseResult autoParseFile(const QString &path)
{
    const FileFormat format = detectFormat(path);

    QVector<FileFormat> order;
    switch (format) {
    case FileFormat::Pos:
        order = {FileFormat::Pos, FileFormat::Gga, FileFormat::Tabular};
        break;
    case FileFormat::Gga:
        order = {FileFormat::Gga, FileFormat::Pos, FileFormat::Tabular};
        break;
    case FileFormat::Tabular:
        order = {FileFormat::Tabular, FileFormat::Gga, FileFormat::Pos};
        break;
    default:
        order = {FileFormat::Gga, FileFormat::Pos, FileFormat::Tabular};
        break;
    }

    ParseResult last;
    bool first = true;
    for (FileFormat candidate : order) {
        ParseResult current;
        switch (candidate) {
        case FileFormat::Gga:
            current = parseGgaFile(path);
            break;
        case FileFormat::Pos:
            current = parsePosFile(path);
            break;
        case FileFormat::Tabular:
            current = parseTabularFile(path);
            break;
        default:
            continue;
        }

        if (current.ok) {
            if (!first) {
                current.warnings.prepend(
                    QStringLiteral("按扩展名推断的格式解析失败，已回退为 %1 解析").arg(formatName(candidate)));
            }
            return current;
        }
        if (first) {
            last = current;
            first = false;
        }
    }

    if (last.error.isEmpty()) {
        last.error = QStringLiteral("无法识别文件格式: %1").arg(path);
    }
    return last;
}

} // namespace gnss
