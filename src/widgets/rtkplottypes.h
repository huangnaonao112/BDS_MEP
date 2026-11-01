#ifndef RTKPLOTTYPES_H
#define RTKPLOTTYPES_H

#include <QVector>
#include <QPointF>
#include <QColor>
#include <QHash>
#include <climits>
#include <cstdint>
#include <QMetaType>

// 绘图类型枚举
enum PlotType {
    PLOT_TRK = 0      // 轨迹绘图 (Gnd trk)
};

// 轨迹数据结构
struct TrajectoryPoint {
    double x, y, z;      // 坐标 (m)
    double time;         // 时间戳 (GPS秒)
    int quality;         // 质量指标 (0-9)
    int type;            // 点类型 (0=fixed, 1=float, 2=single)
    double sdx, sdy, sdz; // 标准差 (m)
    double sdxy, sdyz, sdzx; // 协方差 (m²)
    double hdop;         // 水平精度因子
    
    TrajectoryPoint() : x(0), y(0), z(0), time(0), quality(0), type(0), 
                       sdx(0), sdy(0), sdz(0), sdxy(0), sdyz(0), sdzx(0), hdop(0) {}
    TrajectoryPoint(double x_, double y_, double z_, double t_, int q_ = 0, int type_ = 0,
                   double sdx_ = 0, double sdy_ = 0, double sdz_ = 0,
                   double sdxy_ = 0, double sdyz_ = 0, double sdzx_ = 0, double hdop_ = 0)
        : x(x_), y(y_), z(z_), time(t_), quality(q_), type(type_),
          sdx(sdx_), sdy(sdy_), sdz(sdz_), sdxy(sdxy_), sdyz(sdyz_), sdzx(sdzx_), hdop(hdop_) {}
};

// 速度箭头结构体定义
struct VelocityArrow {
    QPointF pos;
    double angle;
};

// 空间索引结构用于视口裁剪优化
struct SpatialIndex {
    int gridSize;                           // 网格大小
    double gridCellWidth, gridCellHeight;   // 网格单元宽度和高度
    double minX, minY, maxX, maxY;          // 边界
    QHash<uint64_t, QVector<int>> grid;     // 网格索引 (hash -> point indices)
    
    SpatialIndex() : gridSize(100), gridCellWidth(0), gridCellHeight(0),
                     minX(0), minY(0), maxX(0), maxY(0) {}
};

// 更详细的性能监控指标
struct PerformanceMetrics {
    int spatialIndexBuildTime; // 空间索引构建时间（毫秒）
    int viewportCullingTime;   // 视口裁剪时间（毫秒）
    int coordinateTransformTime; // 坐标转换时间（毫秒）
    int trajectoryDrawTime;    // 轨迹绘制时间（毫秒）
    int gridDrawTime;          // 网格绘制时间（毫秒）
    int markDrawTime;          // 标记绘制时间（毫秒）
    int velocityArrowTime;     // 速度箭头绘制时间（毫秒）
    
    // 性能统计
    int maxRenderTime;         // 最大渲染时间（毫秒）
    int minRenderTime;         // 最小渲染时间（毫秒）
    double avgRenderTime;      // 平均渲染时间（毫秒）
    int renderCount;           // 渲染次数统计
    
    PerformanceMetrics() {
        reset();
    }
    
    void reset() {
        spatialIndexBuildTime = 0;
        viewportCullingTime = 0;
        coordinateTransformTime = 0;
        trajectoryDrawTime = 0;
        gridDrawTime = 0;
        markDrawTime = 0;
        velocityArrowTime = 0;
        maxRenderTime = 0;
        minRenderTime = INT_MAX;
        avgRenderTime = 0.0;
        renderCount = 0;
    }
};

Q_DECLARE_METATYPE(TrajectoryPoint)

#endif // RTKPLOTTYPES_H
