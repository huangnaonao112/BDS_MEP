#include "camera2d.h"

#include <algorithm>
#include <cmath>

Camera2D::Camera2D(const QPointF &center, double scale, const QSize &viewport)
    : m_center(center)
    , m_scale(scale)
    , m_viewport(viewport)
{
}

QRectF Camera2D::visibleWorld() const
{
    if (!isValid()) {
        return QRectF();
    }

    const QPointF topLeft = screenToWorld(QPointF(0.0, 0.0));
    const QPointF bottomRight = screenToWorld(QPointF(m_viewport.width(), m_viewport.height()));
    return QRectF(topLeft, bottomRight).normalized();
}

QPointF Camera2D::worldToScreen(const QPointF &world) const
{
    const double halfW = m_viewport.width() / 2.0;
    const double halfH = m_viewport.height() / 2.0;
    return QPointF(halfW + (world.x() - m_center.x()) * m_scale,
                   halfH - (world.y() - m_center.y()) * m_scale);
}

QPointF Camera2D::screenToWorld(const QPointF &screen) const
{
    const double halfW = m_viewport.width() / 2.0;
    const double halfH = m_viewport.height() / 2.0;
    return QPointF(m_center.x() + (screen.x() - halfW) / m_scale,
                   m_center.y() - (screen.y() - halfH) / m_scale);
}

void Camera2D::panByPixels(const QPointF &delta)
{
    if (m_scale <= 0.0) {
        return;
    }
    // 拖拽方向与地图一致：向右拖，视野中心左移
    m_center.setX(m_center.x() - delta.x() / m_scale);
    m_center.setY(m_center.y() + delta.y() / m_scale);
}

void Camera2D::zoomAt(const QPointF &screenAnchor, double factor)
{
    if (!isValid() || factor <= 0.0) {
        return;
    }

    const QPointF anchorWorld = screenToWorld(screenAnchor);
    m_scale *= factor;
    if (m_scale <= 0.0) {
        return;
    }

    const double halfW = m_viewport.width() / 2.0;
    const double halfH = m_viewport.height() / 2.0;
    m_center.setX(anchorWorld.x() - (screenAnchor.x() - halfW) / m_scale);
    m_center.setY(anchorWorld.y() + (screenAnchor.y() - halfH) / m_scale);
}

bool Camera2D::operator==(const Camera2D &other) const
{
    return m_center == other.m_center && m_scale == other.m_scale
           && m_viewport == other.m_viewport;
}

Camera2D Camera2D::fit(const QRectF &worldBounds, const QSize &viewport, double paddingPixels)
{
    Camera2D camera(QPointF(0.0, 0.0), 1.0, viewport);
    if (!viewport.isValid() || worldBounds.isNull()) {
        return camera;
    }

    double width = worldBounds.width();
    double height = worldBounds.height();
    // 单点/一条直线：给一个默认视野，避免除零
    if (width < 1e-6) {
        width = 100.0;
    }
    if (height < 1e-6) {
        height = 100.0;
    }

    const double usableW = std::max(10.0, viewport.width() - 2.0 * paddingPixels);
    const double usableH = std::max(10.0, viewport.height() - 2.0 * paddingPixels);
    const double scale = std::min(usableW / width, usableH / height);

    camera.setCenter(worldBounds.center());
    camera.setScale(scale > 0.0 ? scale : 1.0);
    return camera;
}

Camera2D Camera2D::interpolate(const Camera2D &from, const Camera2D &to, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    Camera2D result = to;
    result.setCenter(from.center() * (1.0 - t) + to.center() * t);
    result.setScale(from.scale() * (1.0 - t) + to.scale() * t);
    return result;
}
