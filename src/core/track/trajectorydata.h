#ifndef TRAJECTORYDATA_H
#define TRAJECTORYDATA_H

#include <QString>
#include <QVector>

#include "epochdata.h"
#include "trackmeta.h"

// 轨迹点：x/y/z 为相对首个历元的 ENU 坐标（米），供绘图与表格使用。
struct CoordinatePoint {
    double x = 0.0;          // East (m)
    double y = 0.0;          // North (m)
    double z = 0.0;          // Up (m)
    double time = 0.0;       // 原始时间（绝对秒或当天秒）
    int quality = 0;         // 标准质量码
    int satellites = 0;
    double hdop = 0.0;
    QString sourceType;      // GGA / POS / TABULAR
    double sdx = 0.0, sdy = 0.0, sdz = 0.0;
    double sdxy = 0.0, sdyz = 0.0, sdzx = 0.0;
};

// UI 侧的轨迹容器，只做"存点 + 统计"，不再负责坐标转换。
class TrajectoryData
{
public:
    void clear();
    void addPoint(const CoordinatePoint &point);

    const QVector<CoordinatePoint> &getPoints() const { return m_points; }
    int getPointCount() const { return m_points.size(); }

    // 由解析后的历元序列构建：以首个历元为原点转 ENU。
    bool buildFromSeries(const gnss::EpochSeries &series, QString *error = nullptr);

    QString getStartTime() const;
    QString getEndTime() const;
    QString getSampleInterval() const;
    int getEpochCount() const;
    QString getCompleteness() const;
    QString getFixRate() const;

    TrackMeta meta() const;

private:
    QVector<CoordinatePoint> m_points;
    bool m_hasDate = false;   // 时间是否带日期，决定时间文本格式
    double m_intervalSeconds = 0.0;
    double m_spanSeconds = 0.0;
};

#endif // TRAJECTORYDATA_H
