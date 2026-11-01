#ifndef PLOT_TRACKSERIES_H
#define PLOT_TRACKSERIES_H

#include <QRectF>
#include <QVector>

#include "rtkplottypes.h"

// 不可变轨迹点集 + 版本号。
//
// 数据层只负责存点和算包围盒，不再回答"哪些点可见"——那是
// TrackIndex/TrackLod 的职责。revision 用于缓存失效：渲染缓存只认
// (revision, camera)，不靠手工置 bool。
class TrackSeries
{
public:
    void setPoints(const QVector<TrajectoryPoint> &points);
    void append(const TrajectoryPoint &point);
    void clear();

    const QVector<TrajectoryPoint> &points() const { return m_points; }
    int size() const { return m_points.size(); }
    bool isEmpty() const { return m_points.isEmpty(); }

    QRectF bounds() const { return m_bounds; }
    quint64 revision() const { return m_revision; }

private:
    void updateBounds();

    QVector<TrajectoryPoint> m_points;
    QRectF m_bounds;
    quint64 m_revision = 0;
};

#endif // PLOT_TRACKSERIES_H
