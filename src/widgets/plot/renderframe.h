#ifndef PLOT_RENDERFRAME_H
#define PLOT_RENDERFRAME_H

#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QVector>

// 一帧的渲染数据：所有几何都已换算到屏幕坐标，渲染层只负责落笔 + 裁剪。
// 这是"几何策略"和"绘制"之间的唯一接口，也是可脱离 QPainter 测试的部分。
struct FrameMarker {
    QPointF position;
    int quality = 0;
};

struct FrameEllipse {
    QPolygonF polygon;
};

struct FrameArrow {
    QPointF position;
    double angleDegrees = 0.0;
    int size = 15;
};

struct RenderFrame {
    QVector<QPolygonF> polylines;   // 已按时间断点分段
    QVector<FrameMarker> markers;
    QVector<FrameEllipse> ellipses;
    QVector<FrameArrow> arrows;

    bool hasStart = false;
    QPointF startPosition;

    QRectF clipRect;                // 画布矩形，渲染时作为裁剪区
    quint64 seriesRevision = 0;

    bool isEmpty() const
    {
        return polylines.isEmpty() && markers.isEmpty() && ellipses.isEmpty()
               && arrows.isEmpty() && !hasStart;
    }
};

#endif // PLOT_RENDERFRAME_H
