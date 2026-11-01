#ifndef GNSS_COORDINATES_H
#define GNSS_COORDINATES_H

// WGS-84 坐标转换。
//
// 全工程只此一份实现：解析器、评估引擎、轨迹图都调用这里，
// 不再各自复制 blh2ecef / ecef2blh / ecef2enu。

namespace gnss {

struct Blh {
    double lat = 0.0;     // 纬度 (deg)
    double lon = 0.0;     // 经度 (deg)
    double height = 0.0;  // 椭球高 (m)
};

struct Ecef {
    double x = 0.0, y = 0.0, z = 0.0;  // (m)
};

struct Enu {
    double e = 0.0, n = 0.0, u = 0.0;  // (m)
};

Ecef blhToEcef(const Blh &blh);
Blh ecefToBlh(const Ecef &ecef);

// 以 ref 为原点的 ENU 旋转矩阵（行优先，r[0..2]=E, r[3..5]=N, r[6..8]=U）
void enuRotationMatrix(const Blh &ref, double r[9]);

Enu ecefToEnu(const Blh &ref, const Ecef &ecef);
Ecef enuToEcef(const Blh &ref, const Enu &enu);

double horizontalDistance(const Enu &enu);
bool isFinite(const Blh &blh);
bool isFinite(const Ecef &ecef);

} // namespace gnss

#endif // GNSS_COORDINATES_H
