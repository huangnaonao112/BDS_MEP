#include "trackseries.h"

#include <algorithm>

void TrackSeries::setPoints(const QVector<TrajectoryPoint> &points)
{
    m_points = points;
    updateBounds();
    ++m_revision;
}

void TrackSeries::append(const TrajectoryPoint &point)
{
    m_points.append(point);
    // 增量更新包围盒，避免每次追加都全量扫描
    if (m_points.size() == 1) {
        m_bounds = QRectF(QPointF(point.x, point.y), QSizeF(0.0, 0.0));
    } else {
        const double minX = std::min(m_bounds.left(), point.x);
        const double maxX = std::max(m_bounds.right(), point.x);
        const double minY = std::min(m_bounds.top(), point.y);
        const double maxY = std::max(m_bounds.bottom(), point.y);
        m_bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
    }
    ++m_revision;
}

void TrackSeries::clear()
{
    m_points.clear();
    m_bounds = QRectF();
    ++m_revision;
}

void TrackSeries::updateBounds()
{
    if (m_points.isEmpty()) {
        m_bounds = QRectF();
        return;
    }

    double minX = m_points.first().x;
    double maxX = minX;
    double minY = m_points.first().y;
    double maxY = minY;

    for (const TrajectoryPoint &point : m_points) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }

    m_bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}
