#ifndef GNSS_TIMEUTIL_H
#define GNSS_TIMEUTIL_H

#include <QString>

// 时间模型（全工程唯一实现）
//
// 关键约定：历元时间统一存成**一个 double**
//   * 文件里带日期（RTKLIB .pos、表格文件）  -> UNIX 纪元起的绝对秒，hasDate=true
//   * 文件里只有时刻（NMEA GGA）             -> 当天秒 (0..86400)，hasDate=false
//
// 旧实现把日期丢掉、只保留"当天秒"，再在各处用 ±86400 / fmod(sec-18+86400)
// 打补丁，跨零点就会错。现在由 EpochSeries 记录 hasDate，对齐时按同一口径
// 比较，见 epochmatch 的相关说明。
namespace gnss {

// GPS 周 + 周内秒 -> UNIX 秒（未扣跳秒，调用方按需自行处理）
double gpsToUnixSeconds(int week, double timeOfWeek);

// "084246.987" -> 30566.987
bool parseNmeaTimeOfDay(const QString &text, double *seconds);

// "08:42:46.987" / "8:42:46" -> 30566.987
bool parseClock(const QString &text, double *seconds);

// "2025/07/02" / "2025-07-02" / "20250702" -> UNIX 纪元起的**天**偏移
bool parseDateDayIndex(const QString &text, double *dayIndex);

// 日期 + 时刻 -> 绝对秒（UTC）
bool parseDateTime(const QString &date, const QString &clock, double *seconds);

double dayIndexToUnixSeconds(double dayIndex);
QString formatAbsoluteTime(double unixSeconds, int decimals = 3);
QString formatTimeOfDay(double seconds, int decimals = 3);

// 时间范围显示：带日期的给全 "yyyy-MM-dd HH:mm:ss"，否则只给 "HH:mm:ss"
QString formatTimeRange(double begin, double end, bool hasDate);

double wrapToDay(double seconds);

// 环形差：把 a-b 折算到 (-43200, 43200]
double circularDifference(double a, double b);

} // namespace gnss

#endif // GNSS_TIMEUTIL_H
