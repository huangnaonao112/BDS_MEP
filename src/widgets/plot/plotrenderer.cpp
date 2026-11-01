#include "plotrenderer.h"

#include <QFont>
#include <QLineF>
#include <QPainterPath>
#include <QPolygonF>
#include <QTransform>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace {

static const QColor kQualityColor[] = {
    Qt::white,      // 0: no solution
    Qt::green,      // 1: fix
    Qt::yellow,     // 2: float
    Qt::blue,       // 3: SBAS
    Qt::magenta,    // 4: DGPS
    Qt::red,        // 5: single
    Qt::cyan,       // 6: PPP
    Qt::gray        // 7: reserved
};

// 裁剪守卫：轨迹/标记允许画到画布外，由裁剪矩形切齐。
struct ClipGuard {
    QPainter &painter;
    ClipGuard(QPainter &p, const QRectF &rect) : painter(p)
    {
        painter.save();
        painter.setClipRect(rect);
    }
    ~ClipGuard() { painter.restore(); }
};

} // namespace

double PlotRenderer::autoTick(double scale)
{
    const double candidates[] = {1.0, 2.0, 5.0, 10.0};
    if (scale <= 0.0) {
        return 1.0;
    }

    const double tick = 50.0 / scale;   // 目标是每 50 像素一个刻度
    const double order = std::pow(10.0, std::floor(std::log10(tick)));
    for (double candidate : candidates) {
        if (tick <= candidate * order) {
            return candidate * order;
        }
    }
    return 10.0 * order;
}

QString PlotRenderer::formatLabel(double value, double spacing)
{
    const double absValue = std::fabs(value);
    if (absValue < 1e-12) {
        return QStringLiteral("0");
    }

    int precision = 0;
    if (spacing < 1.0) {
        precision = static_cast<int>(std::ceil(-std::log10(spacing)));
    }

    if (absValue > 1e6 || (absValue < 1e-3 && absValue > 0.0)) {
        return QString::number(value, 'e', 2);
    }
    return QString::number(value, 'f', std::clamp(precision, 0, 6));
}

void PlotRenderer::paintGrid(QPainter &painter, const Camera2D &camera, const PlotStyle &style)
{
    const double scale = camera.scale();
    if (scale < 1e-12 || scale > 1e12) {
        return;
    }

    double worldSpacing = autoTick(scale);
    worldSpacing = std::clamp(worldSpacing, 1e-9, 1e9);
    if (!style.highQuality()) {
        worldSpacing *= 2.0;
    }

    const QSize viewport = camera.viewport();
    const double width = viewport.width();
    const double height = viewport.height();
    const double centerX = camera.center().x();
    const double centerY = camera.center().y();

    const double halfWidth = width / (2.0 * scale);
    const double halfHeight = height / (2.0 * scale);
    const double startX = std::floor((centerX - halfWidth) / worldSpacing) * worldSpacing;
    const double startY = std::floor((centerY - halfHeight) / worldSpacing) * worldSpacing;

    const double screenStartX = width / 2.0 + (startX - centerX) * scale;
    const double screenStartY = height / 2.0 - (startY - centerY) * scale;
    const double screenSpacing = worldSpacing * scale;

    painter.setPen(QPen(style.gridColor, 1.0, Qt::SolidLine));

    QVector<QLineF> lines;
    lines.reserve(100);
    for (double sx = screenStartX; sx <= width + 1.0; sx += screenSpacing) {
        if (sx < -1.0) continue;
        lines.append(QLineF(sx, 0, sx, height));
    }
    for (double sy = screenStartY; sy >= -1.0; sy -= screenSpacing) {
        if (sy > height + 1.0) continue;
        lines.append(QLineF(0, sy, width, sy));
    }
    painter.drawLines(lines);

    // 世界原点十字线
    const double originSx = width / 2.0 - centerX * scale;
    const double originSy = height / 2.0 + centerY * scale;
    if ((originSx >= -10.0 && originSx <= width + 10.0)
        || (originSy >= -10.0 && originSy <= height + 10.0)) {
        painter.setPen(QPen(Qt::red, 1.5, Qt::DashLine));
        if (originSx >= 0 && originSx <= width) {
            painter.drawLine(QLineF(originSx, 0, originSx, height));
        }
        if (originSy >= 0 && originSy <= height) {
            painter.drawLine(QLineF(0, originSy, width, originSy));
        }
        if (originSx >= 0 && originSx <= width && originSy >= 0 && originSy <= height) {
            painter.setBrush(Qt::red);
            painter.drawEllipse(QPointF(originSx, originSy), 3, 3);
            painter.drawText(QPointF(originSx + 5, originSy - 5), QStringLiteral("Origin (0,0)"));
        }
    }

    if (style.highQuality()) {
        const double mainSpacing = worldSpacing * 5.0;
        const double screenMainSpacing = mainSpacing * scale;
        const double mainStartX = width / 2.0
                                  + (std::floor((centerX - halfWidth) / mainSpacing) * mainSpacing - centerX) * scale;
        const double mainStartY = height / 2.0
                                  - (std::floor((centerY - halfHeight) / mainSpacing) * mainSpacing - centerY) * scale;

        painter.setPen(QPen(style.gridColor, 2.0));
        lines.clear();
        for (double sx = mainStartX; sx <= width + 1.0; sx += screenMainSpacing) {
            if (sx < -1.0) continue;
            lines.append(QLineF(sx, 0, sx, height));
        }
        for (double sy = mainStartY; sy >= -1.0; sy -= screenMainSpacing) {
            if (sy > height + 1.0) continue;
            lines.append(QLineF(0, sy, width, sy));
        }
        painter.drawLines(lines);
    }
}

void PlotRenderer::paintAxes(QPainter &painter, const Camera2D &camera, const PlotStyle &style)
{
    const QSize viewport = camera.viewport();
    const double width = viewport.width();
    const double height = viewport.height();
    const double scale = camera.scale();
    if (scale <= 0.0) {
        return;
    }

    painter.setPen(QPen(style.textColor, 1));
    painter.drawLine(0, height, width, height);
    painter.drawLine(0, 0, 0, height);

    const double worldSpacing = autoTick(scale / 4.0);
    const double halfWidth = width / (2.0 * scale);
    const double halfHeight = height / (2.0 * scale);
    const double startX = std::floor((camera.center().x() - halfWidth) / worldSpacing) * worldSpacing;
    const double startY = std::floor((camera.center().y() - halfHeight) / worldSpacing) * worldSpacing;

    const double screenStartX = width / 2.0 + (startX - camera.center().x()) * scale;
    const double screenStartY = height / 2.0 - (startY - camera.center().y()) * scale;
    const double screenSpacing = worldSpacing * scale;

    QFont labelFont(painter.font().family(), 10, QFont::Bold);
    painter.setFont(labelFont);

    int i = 0;
    for (double sx = screenStartX; sx <= width + 1.0; sx += screenSpacing, ++i) {
        if (sx < -1.0) continue;
        painter.drawLine(static_cast<int>(sx + 0.5), height,
                         static_cast<int>(sx + 0.5), height - 8);
        if (style.highQuality() || (i % 2 == 0)) {
            const double x = startX + i * worldSpacing;
            painter.drawText(static_cast<int>(sx + 0.5) - 60, height - 25, 120, 20,
                             Qt::AlignCenter, formatLabel(x, worldSpacing));
        }
    }

    int j = 0;
    for (double sy = screenStartY; sy >= -1.0; sy -= screenSpacing, ++j) {
        if (sy > height + 1.0) continue;
        painter.drawLine(0, static_cast<int>(sy + 0.5), 8, static_cast<int>(sy + 0.5));
        if (style.highQuality() || (j % 2 == 0)) {
            const double y = startY + j * worldSpacing;
            painter.drawText(12, static_cast<int>(sy + 0.5) - 10, 100, 20,
                             Qt::AlignLeft | Qt::AlignVCenter, formatLabel(y, worldSpacing));
        }
    }
}

void PlotRenderer::paintScale(QPainter &painter, const Camera2D &camera, const PlotStyle &style)
{
    const QSize viewport = camera.viewport();
    const double width = viewport.width();
    const double height = viewport.height();

    double worldLength = autoTick(camera.scale());
    if (!style.highQuality()) {
        worldLength *= 2.0;
    }
    const int screenLength = static_cast<int>(worldLength * camera.scale());

    QString label;
    if (worldLength < 0.001) {
        label = QStringLiteral("%1 um").arg(worldLength * 1e6, 0, 'f', 0);
    } else if (worldLength < 0.01) {
        label = QStringLiteral("%1 mm").arg(worldLength * 1000.0, 0, 'f', 0);
    } else if (worldLength < 1.0) {
        label = QStringLiteral("%1 cm").arg(worldLength * 100.0, 0, 'f', 0);
    } else if (worldLength < 1000.0) {
        label = QStringLiteral("%1 m").arg(worldLength, 0, 'f', 0);
    } else {
        label = QStringLiteral("%1 km").arg(worldLength / 1000.0, 0, 'f', 0);
    }

    const int marginX = 20;
    const int marginY = 25;
    const int scaleY = static_cast<int>(height) - marginY;
    const int scaleEndX = static_cast<int>(width) - marginX;
    const int scaleStartX = scaleEndX - screenLength;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(style.textColor, 1.2));
    painter.drawLine(scaleStartX, scaleY, scaleEndX, scaleY);
    painter.drawLine(scaleStartX, scaleY - 5, scaleStartX, scaleY + 5);
    painter.drawLine(scaleEndX, scaleY - 5, scaleEndX, scaleY + 5);

    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    painter.drawText(QRectF(scaleStartX, scaleY - 20, screenLength, 15), Qt::AlignCenter, label);
    painter.restore();
}

void PlotRenderer::paintFrame(QPainter &painter, const RenderFrame &frame, const PlotStyle &style)
{
    if (frame.isEmpty()) {
        return;
    }

    ClipGuard clip(painter, frame.clipRect);

    if (style.showTrack) {
        painter.setPen(QPen(style.trackColor, style.lineWidth));
        for (const QPolygonF &polyline : frame.polylines) {
            painter.drawPolyline(polyline);
        }
    }

    if (style.showMarkers && !frame.markers.isEmpty()) {
        QVector<QPointF> qualityPoints[8];
        for (const FrameMarker &marker : frame.markers) {
            const int q = std::clamp(marker.quality, 0, 7);
            qualityPoints[q].append(marker.position);
        }

        if (style.markerSize <= 2.5) {
            for (int q = 0; q < 8; ++q) {
                if (qualityPoints[q].isEmpty()) continue;
                painter.setPen(QPen(kQualityColor[q], style.markerSize));
                painter.drawPoints(qualityPoints[q].data(), qualityPoints[q].size());
            }
        } else {
            for (int q = 0; q < 8; ++q) {
                if (qualityPoints[q].isEmpty()) continue;
                painter.setPen(q == 1 ? QPen(QColor(0x00, 0xCC, 0x00), 1)
                                      : QPen(style.background, 1));
                painter.setBrush(kQualityColor[q]);
                for (const QPointF &point : qualityPoints[q]) {
                    painter.drawEllipse(point, style.markerSize, style.markerSize);
                }
            }
        }
    }

    if (style.highQuality() && frame.hasStart) {
        painter.setPen(QPen(Qt::blue, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(frame.startPosition, 8, 8);
        painter.drawText(frame.startPosition + QPointF(10, -10), QStringLiteral("START"));
    }

    if (!frame.ellipses.isEmpty()) {
        painter.setPen(QPen(style.gridColor, 1));
        painter.setBrush(Qt::NoBrush);
        for (const FrameEllipse &ellipse : frame.ellipses) {
            painter.drawPolyline(ellipse.polygon);
        }
    }

    for (const FrameArrow &arrow : frame.arrows) {
        drawArrowMarker(painter, arrow.position, style.trackColor, arrow.size,
                        static_cast<int>(arrow.angleDegrees));
    }
}

void PlotRenderer::drawArrowMarker(QPainter &painter, const QPointF &pos, const QColor &color,
                                   int size, int rotation)
{
    const double s = static_cast<double>(size);
    const double h = s * 0.5;
    const double q = s * 0.25;

    QPainterPath path;
    path.moveTo(0, -h);
    path.lineTo(q, 0);
    path.lineTo(-q, 0);
    path.closeSubpath();
    path.addRect(-q * 0.3, 0, q * 0.6, h);

    QTransform transform;
    transform.translate(pos.x(), pos.y());
    transform.rotate(rotation);

    painter.save();
    painter.setPen(QPen(color, 1));
    painter.setBrush(color);
    painter.drawPath(transform.map(path));
    painter.restore();
}
