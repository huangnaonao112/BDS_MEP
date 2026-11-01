#include "epochdata.h"
#include "gnssdefs.h"
#include "timeutil.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace gnss {

void ensureBlh(GnssEpoch &epoch)
{
    if (epoch.hasBlh) {
        return;
    }
    if (!epoch.hasEcef) {
        return;
    }
    const Blh blh = ecefToBlh(Ecef{epoch.x, epoch.y, epoch.z});
    epoch.lat = blh.lat;
    epoch.lon = blh.lon;
    epoch.height = blh.height;
    epoch.hasBlh = true;
}

void ensureEcef(GnssEpoch &epoch)
{
    if (epoch.hasEcef) {
        return;
    }
    if (!epoch.hasBlh) {
        return;
    }
    const Ecef ecef = blhToEcef(Blh{epoch.lat, epoch.lon, epoch.height});
    epoch.x = ecef.x;
    epoch.y = ecef.y;
    epoch.z = ecef.z;
    epoch.hasEcef = true;
}

EpochSeries thinSeries(const EpochSeries &series, double interval)
{
    if (interval <= 0.0 || series.epochs.isEmpty()) {
        return series;
    }

    EpochSeries result = series;
    result.epochs.clear();
    result.epochs.reserve(series.epochs.size());

    double lastKept = -1e18;
    for (const GnssEpoch &epoch : series.epochs) {
        if (epoch.time - lastKept < interval - 0.001) {
            continue;
        }
        result.epochs.append(epoch);
        lastKept = epoch.time;
    }
    return result;
}

QStringList normalizeSeries(EpochSeries &series)
{
    QStringList notes;

    if (series.epochs.isEmpty()) {
        return notes;
    }

    // 1. 补齐坐标表示
    for (GnssEpoch &epoch : series.epochs) {
        ensureBlh(epoch);
        ensureEcef(epoch);
    }

    // 2. 无日期数据：先在**文件顺序**上做跨零点展开。
    //    必须先展开再排序：00:00:01 与 23:59:59 直接按数值排序会把整段轨迹拆散。
    if (!series.hasDate) {
        int wraps = 0;
        double previousRaw = series.epochs.first().time;
        for (int i = 1; i < series.epochs.size(); ++i) {
            const double raw = series.epochs[i].time;
            if (raw < previousRaw - kHalfDay) {
                ++wraps;
            }
            previousRaw = raw;
            if (wraps > 0) {
                series.epochs[i].time = raw + kSecondsPerDay * wraps;
            }
        }
        if (wraps > 0) {
            notes << QStringLiteral("检测到 %1 次跨零点，已按连续时间展开").arg(wraps);
        }
    }

    // 3. 稳定排序（同时间保持原顺序）
    bool sorted = true;
    for (int i = 1; i < series.epochs.size(); ++i) {
        if (series.epochs[i].time < series.epochs[i - 1].time) {
            sorted = false;
            break;
        }
    }
    if (!sorted) {
        std::stable_sort(series.epochs.begin(), series.epochs.end(),
                         [](const GnssEpoch &a, const GnssEpoch &b) { return a.time < b.time; });
        notes << QStringLiteral("文件内历元并非按时间排列，已按时间重新排序");
    }

    return notes;
}

double seriesSpanSeconds(const EpochSeries &series)
{
    if (series.epochs.size() < 2) {
        return 0.0;
    }
    return series.epochs.last().time - series.epochs.first().time;
}

double medianSampleInterval(const EpochSeries &series)
{
    if (series.epochs.size() < 2) {
        return 0.0;
    }

    QVector<double> diffs;
    diffs.reserve(series.epochs.size() - 1);
    for (int i = 1; i < series.epochs.size(); ++i) {
        const double diff = series.epochs[i].time - series.epochs[i - 1].time;
        // 丢弃明显不合理的长间隔（>1h）与重复时间
        if (diff > 1e-6 && diff < 3600.0) {
            diffs.append(diff);
        }
    }

    if (diffs.isEmpty()) {
        return 0.0;
    }

    const int mid = diffs.size() / 2;
    std::nth_element(diffs.begin(), diffs.begin() + mid, diffs.end());
    return diffs[mid];
}

} // namespace gnss
