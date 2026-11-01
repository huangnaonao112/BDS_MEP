#ifndef GNSS_EPOCHDATA_H
#define GNSS_EPOCHDATA_H

#include <QString>
#include <QVector>

#include "coordinates.h"

// 解析后的历元（不依赖 UI，可在任意线程构造）：
// 同一份数据同时保留 BLH 与 ECEF，谁需要谁取，避免反复换算。
namespace gnss {

struct GnssEpoch {
    double time = 0.0;      // 绝对秒（hasDate）或当天秒（无日期），见 timeutil.h
    double lat = 0.0;
    double lon = 0.0;
    double height = 0.0;
    double x = 0.0;         // ECEF (m)
    double y = 0.0;
    double z = 0.0;
    bool hasBlh = false;
    bool hasEcef = false;

    int quality = 0;        // 标准质量码，见 gnssdefs.h
    int satellites = 0;
    double hdop = 0.0;
    double sdx = 0.0, sdy = 0.0, sdz = 0.0;
    double sdxy = 0.0, sdyz = 0.0, sdzx = 0.0;
};

struct EpochSeries {
    QVector<GnssEpoch> epochs;
    bool hasDate = false;
    QString format;         // "GGA" / "POS" / "TABULAR"
    QString sourceFile;
    int totalLines = 0;
    int skippedLines = 0;

    bool isEmpty() const { return epochs.isEmpty(); }
    int size() const { return epochs.size(); }
};

void ensureBlh(GnssEpoch &epoch);
void ensureEcef(GnssEpoch &epoch);

// 按时间间隔抽稀（保留每段的首个历元），interval<=0 时原样返回
EpochSeries thinSeries(const EpochSeries &series, double interval);

// 规范化为"时间单调递增"的序列：
//   * 按时间稳定排序；
//   * 无日期时做跨零点展开（23:59:59 -> 00:00:01 变成连续秒），
//     之后的差值就是真实秒差，不再需要任何 ±86400 补丁；
//   * 补齐 BLH/ECEF 表示。
// 返回需要写进日志的提示（可能为空）。
QStringList normalizeSeries(EpochSeries &series);

// 序列时长（秒）
double seriesSpanSeconds(const EpochSeries &series);

// 中位数采样间隔（秒），少于 2 个历元返回 0
double medianSampleInterval(const EpochSeries &series);

} // namespace gnss

#endif // GNSS_EPOCHDATA_H
