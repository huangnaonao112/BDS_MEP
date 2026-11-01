#ifndef GNSS_GNSSDEFS_H
#define GNSS_GNSSDEFS_H

#include <QString>
#include <QtGlobal>

// GNSS 领域常量与解状态定义。
//
// 本文件是全工程唯一的"解状态"与"椭球参数"来源：
//   * 解析器只负责把文件字段翻译成这里的标准质量码；
//   * 统计、报告、绘图不再各自重新解释质量码。
//
// 质量码沿用 RTKLIB 的 0..6 约定：
//   0=无效 1=固定解 2=浮点解 3=SBAS 4=差分(DGPS) 5=单点 6=PPP
namespace gnss {

// ---- WGS-84 椭球 ----
constexpr double kA  = 6378137.0;                    // 长半轴 (m)
constexpr double kF  = 1.0 / 298.257223563;          // 扁率
constexpr double kE2 = kF * (2.0 - kF);              // 第一偏心率平方

// ---- 时间 ----
constexpr double kSecondsPerDay      = 86400.0;
constexpr double kHalfDay            = 43200.0;
constexpr double kGpsUnixEpochOffset = 315964800.0;  // 1980-01-06T00:00:00Z 相对 UNIX 纪元
constexpr double kGpsWeekSeconds     = 604800.0;
constexpr double kLeapSeconds        = 18.0;         // GPS-UTC（2017-01-01 起）

// 质量码取值范围（绘图按 0..7 分组着色）
constexpr int kMaxQuality = 7;

enum class Quality : int {
    Invalid = 0,
    Fix     = 1,
    Float   = 2,
    Sbas    = 3,
    Dgps    = 4,
    Single  = 5,
    Ppp     = 6
};

// 把 NMEA GGA 的 quality 字段翻译成上面的标准质量码。
//   GGA: 0=无效 1=GPS/SPS 2=DGPS 3=PPS 4=RTK 固定 5=RTK 浮点 6=推算 7=手动 8=仿真
// 标准: 4->1(固定) 5->2(浮点) 2->4(DGPS) 1->5(单点) 6->6(PPP/推算) 其余->0
int normalizeNmeaQuality(int ggaQuality);

QString qualityName(int quality);       // 固定解 / 浮点解 / ...
QString qualityShortName(int quality);  // 单字简称：固/浮/差/单

bool isFix(int quality);
bool isFloat(int quality);
bool isValid(int quality);

// 绘图分类：0=固定解 1=浮点解 2=单点/其它
int plotTypeOf(int quality);

} // namespace gnss

#endif // GNSS_GNSSDEFS_H
