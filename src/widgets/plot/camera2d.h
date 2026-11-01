#ifndef PLOT_CAMERA2D_H
#define PLOT_CAMERA2D_H

#include <QPointF>
#include <QRectF>
#include <QSize>

// 二维视图相机：世界坐标（米）与屏幕坐标（像素）之间的唯一变换源。
//
// 之前 centerX/centerY/zoom 散落在 RTKPlotWidget 里，动画时又有
// currentCenter/targetCenter 两套值，缓存失效全靠一堆 bool 标志。
// 现在相机是一个纯值对象：可比较、可插值、可单独测试，渲染层只读它。
class Camera2D
{
public:
    Camera2D() = default;
    Camera2D(const QPointF &center, double scale, const QSize &viewport);

    QPointF center() const { return m_center; }
    double scale() const { return m_scale; }
    QSize viewport() const { return m_viewport; }

    void setCenter(const QPointF &center) { m_center = center; }
    void setScale(double scale) { m_scale = scale; }
    void setViewport(const QSize &viewport) { m_viewport = viewport; }

    // 屏幕可见区域对应的世界矩形
    QRectF visibleWorld() const;

    QPointF worldToScreen(const QPointF &world) const;
    QPointF screenToWorld(const QPointF &screen) const;

    // 按屏幕像素平移（拖拽）
    void panByPixels(const QPointF &delta);

    // 以屏幕锚点为中心缩放，锚点下的世界坐标保持不动
    void zoomAt(const QPointF &screenAnchor, double factor);

    bool isValid() const { return m_viewport.isValid() && m_scale > 0.0; }
    bool operator==(const Camera2D &other) const;
    bool operator!=(const Camera2D &other) const { return !(*this == other); }

    // 让 bounds 完整落入视口（保留 padding 像素边距）
    static Camera2D fit(const QRectF &worldBounds, const QSize &viewport,
                        double paddingPixels = 10.0);

    // 线性插值（用于缩放/平移动画）
    static Camera2D interpolate(const Camera2D &from, const Camera2D &to, double t);

private:
    QPointF m_center{0.0, 0.0};
    double m_scale = 1.0;
    QSize m_viewport{0, 0};
};

#endif // PLOT_CAMERA2D_H
