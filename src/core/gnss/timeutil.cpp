#include "timeutil.h"
#include "gnssdefs.h"

#include <QDate>
#include <QDateTime>
#include <QStringList>
#include <QTime>

#include <cmath>

namespace gnss {

double gpsToUnixSeconds(int week, double timeOfWeek)
{
    return static_cast<double>(week) * kGpsWeekSeconds + timeOfWeek + kGpsUnixEpochOffset;
}

bool parseNmeaTimeOfDay(const QString &text, double *seconds)
{
    if (!seconds || text.size() < 6) {
        return false;
    }

    // 固定宽度解析：HHMMSS[.sss]，避免每行都跑正则/流解析
    bool ok = false;
    const int hh = text.mid(0, 2).toInt(&ok);
    if (!ok || hh < 0 || hh > 23) return false;
    const int mm = text.mid(2, 2).toInt(&ok);
    if (!ok || mm < 0 || mm > 59) return false;
    const double ss = text.mid(4).toDouble(&ok);
    if (!ok || ss < 0.0 || ss >= 61.0) return false;

    *seconds = hh * 3600.0 + mm * 60.0 + ss;
    return true;
}

bool parseClock(const QString &text, double *seconds)
{
    if (!seconds) {
        return false;
    }

    const QStringList parts = text.split(QLatin1Char(':'));
    if (parts.size() != 3) {
        return false;
    }

    bool ok = false;
    const int hh = parts[0].toInt(&ok);
    if (!ok || hh < 0 || hh > 23) return false;
    const int mm = parts[1].toInt(&ok);
    if (!ok || mm < 0 || mm > 59) return false;
    const double ss = parts[2].toDouble(&ok);
    if (!ok || ss < 0.0 || ss >= 61.0) return false;

    *seconds = hh * 3600.0 + mm * 60.0 + ss;
    return true;
}

bool parseDateDayIndex(const QString &text, double *dayIndex)
{
    if (!dayIndex || text.isEmpty()) {
        return false;
    }

    const QString trimmed = text.trimmed();
    QDate date = QDate::fromString(trimmed, QStringLiteral("yyyy/MM/dd"));
    if (!date.isValid()) {
        date = QDate::fromString(trimmed, QStringLiteral("yyyy-MM-dd"));
    }
    if (!date.isValid()) {
        date = QDate::fromString(trimmed, QStringLiteral("yyyyMMdd"));
    }
    if (!date.isValid()) {
        return false;
    }

    // 以 1970-01-01 为第 0 天，纯整数运算，不依赖时区
    static const QDate epochDate(1970, 1, 1);
    *dayIndex = static_cast<double>(epochDate.daysTo(date));
    return true;
}

bool parseDateTime(const QString &date, const QString &clock, double *seconds)
{
    double dayIndex = 0.0;
    double timeOfDay = 0.0;
    if (!parseDateDayIndex(date, &dayIndex) || !parseClock(clock, &timeOfDay)) {
        return false;
    }
    *seconds = dayIndexToUnixSeconds(dayIndex) + timeOfDay;
    return true;
}

double dayIndexToUnixSeconds(double dayIndex)
{
    return dayIndex * kSecondsPerDay;
}

QString formatAbsoluteTime(double unixSeconds, int decimals)
{
    if (!std::isfinite(unixSeconds)) {
        return QStringLiteral("N/A");
    }

    const qint64 whole = static_cast<qint64>(std::floor(unixSeconds));
    QDateTime dt = QDateTime::fromSecsSinceEpoch(whole, Qt::UTC);
    QString text = dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    if (decimals > 0) {
        double fraction = unixSeconds - static_cast<double>(whole);
        if (fraction < 0.0) {
            fraction += 1.0;
        }
        const qint64 ms = static_cast<qint64>(std::llround(fraction * 1000.0));
        if (ms > 0) {
            text += QString(".%1").arg(ms, 3, 10, QLatin1Char('0'));
        }
    }
    return text;
}

QString formatTimeOfDay(double seconds, int decimals)
{
    if (!std::isfinite(seconds)) {
        return QStringLiteral("N/A");
    }

    double t = std::fmod(seconds, kSecondsPerDay);
    if (t < 0.0) {
        t += kSecondsPerDay;
    }

    const int hh = static_cast<int>(t / 3600.0);
    const int mm = static_cast<int>((t - hh * 3600.0) / 60.0);
    double ss = t - hh * 3600.0 - mm * 60.0;

    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;

    // 处理 59.9995 进位成 60.000 的显示问题
    const double scale = std::pow(10.0, decimals);
    ss = std::round(ss * scale) / scale;
    if (ss >= 60.0) {
        return formatTimeOfDay(std::floor(seconds) + 1.0, decimals);
    }

    QString text = QString("%1:%2:%3")
                       .arg(hh, 2, 10, QLatin1Char('0'))
                       .arg(mm, 2, 10, QLatin1Char('0'))
                       .arg(ss, 2 + (decimals > 0 ? decimals + 1 : 0), 'f', decimals,
                            QLatin1Char('0'));
    return text;
}

QString formatTimeRange(double begin, double end, bool hasDate)
{
    if (!std::isfinite(begin) || !std::isfinite(end)) {
        return QStringLiteral("N/A");
    }
    if (hasDate) {
        return formatAbsoluteTime(begin) + QStringLiteral(" - ") + formatAbsoluteTime(end);
    }
    return formatTimeOfDay(begin) + QStringLiteral(" - ") + formatTimeOfDay(end);
}

double wrapToDay(double seconds)
{
    double t = std::fmod(seconds, kSecondsPerDay);
    if (t < 0.0) {
        t += kSecondsPerDay;
    }
    return t;
}

double circularDifference(double a, double b)
{
    double diff = wrapToDay(a) - wrapToDay(b);
    if (diff > kHalfDay) {
        diff -= kSecondsPerDay;
    } else if (diff <= -kHalfDay) {
        diff += kSecondsPerDay;
    }
    return diff;
}

} // namespace gnss
