#include "trajectorydata.h"

#include "coordinates.h"
#include "gnssdefs.h"
#include "timeutil.h"

#include <algorithm>
#include <cmath>

void TrajectoryData::clear()
{
    m_points.clear();
    m_hasDate = false;
    m_intervalSeconds = 0.0;
    m_spanSeconds = 0.0;
}

void TrajectoryData::addPoint(const CoordinatePoint &point)
{
    m_points.append(point);
}

bool TrajectoryData::buildFromSeries(const gnss::EpochSeries &series, QString *error)
{
    clear();

    if (series.epochs.isEmpty()) {
        if (error) *error = QStringLiteral("数据为空");
        return false;
    }

    // 以首个历元为 ENU 原点（与评估引擎共用同一套坐标转换）
    const gnss::EpochSeries &normalized = series;
    gnss::Blh origin;
    {
        const gnss::GnssEpoch &first = normalized.epochs.first();
        gnss::GnssEpoch copy = first;
        gnss::ensureBlh(copy);
        if (!copy.hasBlh) {
            if (error) *error = QStringLiteral("首个历元缺少可用坐标");
            return false;
        }
        origin = gnss::Blh{copy.lat, copy.lon, copy.height};
    }

    m_hasDate = series.hasDate;
    m_points.reserve(normalized.epochs.size());

    const QString source = series.format.isEmpty() ? QStringLiteral("UNKNOWN") : series.format;

    for (const gnss::GnssEpoch &epoch : normalized.epochs) {
        gnss::GnssEpoch copy = epoch;
        gnss::ensureBlh(copy);
        gnss::ensureEcef(copy);
        if (!copy.hasBlh || !copy.hasEcef) {
            continue;
        }

        const gnss::Enu enu = gnss::ecefToEnu(origin, gnss::Ecef{copy.x, copy.y, copy.z});

        CoordinatePoint point;
        point.x = enu.e;
        point.y = enu.n;
        point.z = enu.u;
        point.time = copy.time;
        point.quality = copy.quality;
        point.satellites = copy.satellites;
        point.hdop = copy.hdop;
        point.sourceType = source;
        point.sdx = copy.sdx;
        point.sdy = copy.sdy;
        point.sdz = copy.sdz;
        point.sdxy = copy.sdxy;
        point.sdyz = copy.sdyz;
        point.sdzx = copy.sdzx;
        m_points.append(point);
    }

    if (m_points.isEmpty()) {
        if (error) *error = QStringLiteral("没有可用的坐标点");
        return false;
    }

    m_spanSeconds = m_points.last().time - m_points.first().time;
    if (m_spanSeconds < 0.0) {
        m_spanSeconds = 0.0;
    }

    // 中位数采样间隔：比旧版的算术平均更抗丢包/断点
    QVector<double> diffs;
    diffs.reserve(m_points.size());
    for (int i = 1; i < m_points.size(); ++i) {
        const double diff = m_points[i].time - m_points[i - 1].time;
        if (diff > 1e-6 && diff < 3600.0) {
            diffs.append(diff);
        }
    }
    if (!diffs.isEmpty()) {
        const int mid = diffs.size() / 2;
        std::nth_element(diffs.begin(), diffs.begin() + mid, diffs.end());
        m_intervalSeconds = diffs[mid];
    }

    return true;
}

QString TrajectoryData::getStartTime() const
{
    if (m_points.isEmpty()) {
        return QStringLiteral("--");
    }
    return m_hasDate ? gnss::formatAbsoluteTime(m_points.first().time)
                     : gnss::formatTimeOfDay(m_points.first().time);
}

QString TrajectoryData::getEndTime() const
{
    if (m_points.isEmpty()) {
        return QStringLiteral("--");
    }
    return m_hasDate ? gnss::formatAbsoluteTime(m_points.last().time)
                     : gnss::formatTimeOfDay(m_points.last().time);
}

QString TrajectoryData::getSampleInterval() const
{
    if (m_points.size() < 2 || m_intervalSeconds <= 0.0) {
        return QStringLiteral("--");
    }
    return QString::number(m_intervalSeconds, 'f', 2) + QStringLiteral(" s");
}

int TrajectoryData::getEpochCount() const
{
    return m_points.size();
}

QString TrajectoryData::getCompleteness() const
{
    // 完整率 = 实际历元 / 按中位数间隔推算的应有历元
    // 旧实现恒返回 100%，等于没算。
    if (m_points.size() < 2 || m_intervalSeconds <= 0.0) {
        return QStringLiteral("100.0%");
    }

    const double expected = std::floor(m_spanSeconds / m_intervalSeconds + 0.5) + 1.0;
    if (expected <= 1.0) {
        return QStringLiteral("100.0%");
    }

    double percent = m_points.size() / expected * 100.0;
    if (percent > 100.0) {
        percent = 100.0;
    }
    return QString::number(percent, 'f', 1) + QLatin1Char('%');
}

QString TrajectoryData::getFixRate() const
{
    if (m_points.isEmpty()) {
        return QStringLiteral("0.0%");
    }

    int fixCount = 0;
    for (const CoordinatePoint &point : m_points) {
        if (gnss::isFix(point.quality)) {
            ++fixCount;
        }
    }

    const double rate = static_cast<double>(fixCount) / m_points.size() * 100.0;
    return QString::number(rate, 'f', 1) + QLatin1Char('%');
}

TrackMeta TrajectoryData::meta() const
{
    TrackMeta meta;
    meta.startTime = getStartTime();
    meta.endTime = getEndTime();
    meta.intervalSeconds = m_intervalSeconds;
    meta.epochCount = m_points.size();
    meta.spanSeconds = m_spanSeconds;

    if (m_points.size() >= 2 && m_intervalSeconds > 0.0) {
        const double expected = std::floor(m_spanSeconds / m_intervalSeconds + 0.5) + 1.0;
        meta.completenessPercent = expected > 1.0
                                       ? std::min(100.0, m_points.size() / expected * 100.0)
                                       : 100.0;
    }

    int fixCount = 0;
    for (const CoordinatePoint &point : m_points) {
        if (gnss::isFix(point.quality)) {
            ++fixCount;
        }
    }
    meta.fixRatePercent = m_points.isEmpty()
                              ? 0.0
                              : static_cast<double>(fixCount) / m_points.size() * 100.0;

    return meta;
}
