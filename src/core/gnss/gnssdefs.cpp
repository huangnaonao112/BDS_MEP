#include "gnssdefs.h"

namespace gnss {

int normalizeNmeaQuality(int ggaQuality)
{
    switch (ggaQuality) {
    case 1: return 5;   // GPS 单点 -> Single
    case 2: return 4;   // DGPS -> Dgps
    case 3: return 3;   // PPS -> Sbas 档（仅保留分类，不做精度承诺）
    case 4: return 1;   // RTK 固定 -> Fix
    case 5: return 2;   // RTK 浮点 -> Float
    case 6: return 6;   // 推算/PPP -> Ppp
    default: return 0;  // 0/7/8 及越界值一律视为无效
    }
}

QString qualityName(int quality)
{
    switch (quality) {
    case 0: return QStringLiteral("无效");
    case 1: return QStringLiteral("固定解");
    case 2: return QStringLiteral("浮点解");
    case 3: return QStringLiteral("SBAS");
    case 4: return QStringLiteral("差分解");
    case 5: return QStringLiteral("单点解");
    case 6: return QStringLiteral("PPP");
    default: return QStringLiteral("未知");
    }
}

QString qualityShortName(int quality)
{
    switch (quality) {
    case 1: return QStringLiteral("固");
    case 2: return QStringLiteral("浮");
    case 3: return QStringLiteral("S");
    case 4: return QStringLiteral("差");
    case 5: return QStringLiteral("单");
    case 6: return QStringLiteral("P");
    default: return QStringLiteral("无");
    }
}

bool isFix(int quality)
{
    return quality == 1;
}

bool isFloat(int quality)
{
    return quality == 2;
}

bool isValid(int quality)
{
    return quality >= 1 && quality <= 6;
}

int plotTypeOf(int quality)
{
    if (quality == 1) return 0;             // 固定解
    if (quality == 2 || quality == 6) return 1;  // 浮点解 / PPP
    return 2;                                // 单点、DGPS、SBAS、未知
}

} // namespace gnss
