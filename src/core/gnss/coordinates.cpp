#include "coordinates.h"
#include "gnssdefs.h"

#include <cmath>

namespace gnss {

namespace {
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
}

Ecef blhToEcef(const Blh &blh)
{
    const double latRad = blh.lat * kDegToRad;
    const double lonRad = blh.lon * kDegToRad;
    const double sinLat = std::sin(latRad);
    const double cosLat = std::cos(latRad);

    // 卯酉圈曲率半径
    const double n = kA / std::sqrt(1.0 - kE2 * sinLat * sinLat);

    Ecef ecef;
    ecef.x = (n + blh.height) * cosLat * std::cos(lonRad);
    ecef.y = (n + blh.height) * cosLat * std::sin(lonRad);
    ecef.z = (n * (1.0 - kE2) + blh.height) * sinLat;
    return ecef;
}

Blh ecefToBlh(const Ecef &ecef)
{
    Blh blh;
    const double p = std::sqrt(ecef.x * ecef.x + ecef.y * ecef.y);

    blh.lon = std::atan2(ecef.y, ecef.x) * kRadToDeg;
    if (p < 1e-12) {
        // 极点附近：经度无定义，直接用 z 定纬
        blh.lat = (ecef.z >= 0.0) ? 90.0 : -90.0;
        blh.height = std::fabs(ecef.z) - kA * (1.0 - kF);
        return blh;
    }

    // 迭代求纬度：10 次足以收敛到 1e-12 rad
    double lat = std::atan2(ecef.z, p * (1.0 - kE2));
    double height = 0.0;
    for (int i = 0; i < 12; ++i) {
        const double sinLat = std::sin(lat);
        const double n = kA / std::sqrt(1.0 - kE2 * sinLat * sinLat);
        height = p / std::cos(lat) - n;
        const double next = std::atan2(ecef.z, p * (1.0 - kE2 * n / (n + height)));
        const double delta = std::fabs(next - lat);
        lat = next;
        if (delta < 1e-13) {
            break;
        }
    }

    const double sinLat = std::sin(lat);
    const double n = kA / std::sqrt(1.0 - kE2 * sinLat * sinLat);
    height = p / std::cos(lat) - n;

    blh.lat = lat * kRadToDeg;
    blh.height = height;
    return blh;
}

void enuRotationMatrix(const Blh &ref, double r[9])
{
    const double latRad = ref.lat * kDegToRad;
    const double lonRad = ref.lon * kDegToRad;
    const double sinLat = std::sin(latRad);
    const double cosLat = std::cos(latRad);
    const double sinLon = std::sin(lonRad);
    const double cosLon = std::cos(lonRad);

    //        E                    N                    U
    r[0] = -sinLon;          r[1] =  cosLon;         r[2] = 0.0;
    r[3] = -sinLat * cosLon; r[4] = -sinLat * sinLon; r[5] = cosLat;
    r[6] =  cosLat * cosLon; r[7] =  cosLat * sinLon; r[8] = sinLat;
}

Enu ecefToEnu(const Blh &ref, const Ecef &ecef)
{
    const Ecef refEcef = blhToEcef(ref);
    const double dx = ecef.x - refEcef.x;
    const double dy = ecef.y - refEcef.y;
    const double dz = ecef.z - refEcef.z;

    double r[9];
    enuRotationMatrix(ref, r);

    Enu enu;
    enu.e = r[0] * dx + r[1] * dy + r[2] * dz;
    enu.n = r[3] * dx + r[4] * dy + r[5] * dz;
    enu.u = r[6] * dx + r[7] * dy + r[8] * dz;
    return enu;
}

Ecef enuToEcef(const Blh &ref, const Enu &enu)
{
    double r[9];
    enuRotationMatrix(ref, r);

    // 旋转矩阵正交，转置即逆
    const double dx = r[0] * enu.e + r[3] * enu.n + r[6] * enu.u;
    const double dy = r[1] * enu.e + r[4] * enu.n + r[7] * enu.u;
    const double dz = r[2] * enu.e + r[5] * enu.n + r[8] * enu.u;

    const Ecef origin = blhToEcef(ref);
    return Ecef{origin.x + dx, origin.y + dy, origin.z + dz};
}

double horizontalDistance(const Enu &enu)
{
    return std::sqrt(enu.e * enu.e + enu.n * enu.n);
}

bool isFinite(const Blh &blh)
{
    return std::isfinite(blh.lat) && std::isfinite(blh.lon) && std::isfinite(blh.height);
}

bool isFinite(const Ecef &ecef)
{
    return std::isfinite(ecef.x) && std::isfinite(ecef.y) && std::isfinite(ecef.z);
}

} // namespace gnss
