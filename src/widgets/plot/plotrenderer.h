#ifndef PLOT_PLOTRENDERER_H
#define PLOT_PLOTRENDERER_H

#include <QPainter>

#include "camera2d.h"
#include "plotstyle.h"
#include "renderframe.h"

// 只负责落笔：几何已经由 TrackRenderModel 算好（屏幕坐标），
// 这里不再接触 TrackSeries / 空间索引，也就不会再出现"渲染层自己决定可见性"。
class PlotRenderer
{
public:
    static void paintFrame(QPainter &painter, const RenderFrame &frame, const PlotStyle &style);
    static void paintGrid(QPainter &painter, const Camera2D &camera, const PlotStyle &style);
    static void paintAxes(QPainter &painter, const Camera2D &camera, const PlotStyle &style);
    static void paintScale(QPainter &painter, const Camera2D &camera, const PlotStyle &style);

    static QString formatLabel(double value, double spacing);

private:
    static double autoTick(double scale);
    static void drawArrowMarker(QPainter &painter, const QPointF &pos, const QColor &color,
                                int size, int rotation);
};

#endif // PLOT_PLOTRENDERER_H
