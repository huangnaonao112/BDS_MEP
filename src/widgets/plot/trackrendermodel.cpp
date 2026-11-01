#include "trackrendermodel.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kBreakSeconds = 300.0;   // 与 rtkplot 的 TBRK 一致
constexpr int kPolylineChunk = 2000;
constexpr int kMaxEllipses = 500;
constexpr int kMaxArrows = 100;
constexpr int kMaxMarkersHigh = 5000;
constexpr int kMaxMarkersLow = 1000;

void appendEllipses(RenderFrame &frame, const QVector<TrajectoryPoint> &points,
                    const QVector<int> &indices, const Camera2D &camera)
{
    const double sint[36] = {
         0.0000, 0.1736, 0.3420, 0.5000, 0.6428, 0.7660, 0.8660, 0.9397, 0.9848,
         1.0000, 0.9848, 0.9397, 0.8660, 0.7660, 0.6428, 0.5000, 0.3420, 0.1736,
         0.0000,-0.1736,-0.3420,-0.5000,-0.6428,-0.7660,-0.8660,-0.9397,-0.9848,
        -1.0000,-0.9848,-0.9397,-0.8660,-0.7660,-0.6428,-0.5000,-0.3420,-0.1736};

    const int step = std::max(1, indices.size() / kMaxEllipses);
    const double scale = camera.scale();

    for (int i = 0; i < indices.size(); i += step) {
        const TrajectoryPoint &p = points[indices[i]];
        if (p.quality < 0 || p.quality > 7 || p.sdx <= 0.0 || p.sdy <= 0.0) {
            continue;
        }

        const double a = p.sdxy / std::sqrt(p.sdx);
        const double b2 = p.sdy - a * a;
        if (b2 < 0.0) {
            continue;
        }
        const double b = std::sqrt(b2);
        const double sqrtSdx = std::sqrt(p.sdx);
        const double maxRadius = std::max(sqrtSdx, std::max(std::fabs(a), b)) * scale;
        if (maxRadius < 2.0) {
            continue;
        }

        const QPointF center = camera.worldToScreen(QPointF(p.x, p.y));
        const int angleStep = (maxRadius < 10.0) ? 4 : (maxRadius < 50.0 ? 2 : 1);

        FrameEllipse ellipse;
        ellipse.polygon.reserve(37 / angleStep + 1);
        for (int j = 0; j <= 36; j += angleStep) {
            const int idx = j % 36;
            const double s = sint[idx];
            const double c = sint[(45 - idx) % 36];
            ellipse.polygon.append(QPointF(center.x() + sqrtSdx * c * scale,
                                           center.y() - (a * c + b * s) * scale));
        }
        frame.ellipses.append(ellipse);
    }
}

void appendArrows(RenderFrame &frame, const QVector<TrajectoryPoint> &points,
                  const QVector<int> &indices, const Camera2D &camera)
{
    if (indices.size() < 3) {
        return;
    }

    const int step = std::max(1, indices.size() / kMaxArrows);
    QVector<int> arrowIndices;
    for (int i = 0; i < indices.size(); i += step) {
        arrowIndices.append(indices[i]);
    }
    if (arrowIndices.size() < 3) {
        return;
    }

    for (int i = 1; i < arrowIndices.size() - 1; ++i) {
        const TrajectoryPoint &p1 = points[arrowIndices[i - 1]];
        const TrajectoryPoint &p2 = points[arrowIndices[i]];
        const TrajectoryPoint &p3 = points[arrowIndices[i + 1]];

        const double dx = p3.x - p1.x;
        const double dy = p3.y - p1.y;
        const double dist = std::sqrt(dx * dx + dy * dy);
        const double dt = p3.time - p1.time;
        if (dt == 0.0 || dist / dt < 0.5 || dist < 1e-9) {
            continue;
        }

        const QPointF center = camera.worldToScreen(QPointF(p2.x, p2.y));
        const double offset = 8.0;
        FrameArrow arrow;
        arrow.position = QPointF(center.x() - offset * dy / dist,
                                 center.y() - offset * dx / dist);
        arrow.angleDegrees = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
        arrow.size = 15;
        frame.arrows.append(arrow);
    }
}

} // namespace

RenderFrame TrackRenderModel::build(const TrackSeries &series,
                                    const TrackIndex &index,
                                    const Camera2D &camera,
                                    const LodPolicy &lod,
                                    const RenderOptions &options)
{
    RenderFrame frame;
    frame.seriesRevision = series.revision();
    frame.clipRect = QRectF(QPointF(0.0, 0.0), QSizeF(camera.viewport()));

    if (!camera.isValid() || series.isEmpty() || index.isEmpty()) {
        return frame;
    }

    const QRectF visible = camera.visibleWorld();
    const QVector<IndexRun> runs = index.queryVisible(visible);
    const QVector<int> indices = TrackLod::select(series, runs, lod);
    if (indices.isEmpty()) {
        return frame;
    }

    const QVector<TrajectoryPoint> &points = series.points();
    QVector<QPointF> screen;
    screen.reserve(indices.size());

    QPolygonF polyline;
    polyline.reserve(std::min<int>(kPolylineChunk, indices.size()));

    const auto flush = [&frame, &polyline]() {
        if (polyline.size() >= 2) {
            frame.polylines.append(polyline);
        }
        polyline.clear();
    };

    double previousTime = 0.0;
    for (int i = 0; i < indices.size(); ++i) {
        const TrajectoryPoint &p = points[indices[i]];
        const QPointF point = camera.worldToScreen(QPointF(p.x, p.y));
        screen.append(point);

        if (i > 0 && (p.time - previousTime) > kBreakSeconds) {
            flush();   // 时间断点：不要把两段轨迹连起来
        }
        previousTime = p.time;

        polyline.append(point);
        if (polyline.size() >= kPolylineChunk) {
            frame.polylines.append(polyline);
            polyline.clear();
            polyline.append(point);   // 保留衔接点，折线不断
        }
    }
    flush();

    if (!options.showMarkers) {
        return frame;
    }

    const int markerBudget = options.highQuality ? kMaxMarkersHigh : kMaxMarkersLow;
    const int markerStep = std::max(1, indices.size() / markerBudget);
    for (int i = 0; i < indices.size(); i += markerStep) {
        frame.markers.append(FrameMarker{screen[i], points[indices[i]].quality});
    }
    if (!frame.markers.isEmpty() && (indices.size() - 1) % markerStep != 0) {
        frame.markers.append(FrameMarker{screen.last(), points[indices.last()].quality});
    }

    if (indices.first() == 0) {
        frame.hasStart = true;
        frame.startPosition = screen.first();
    }

    if (options.highQuality && options.showErrorCircles) {
        appendEllipses(frame, points, indices, camera);
    }
    if (options.highQuality && options.showArrows) {
        appendArrows(frame, points, indices, camera);
    }

    return frame;
}
