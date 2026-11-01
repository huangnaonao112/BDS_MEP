#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <random>

#include "camera2d.h"
#include "fileparser.h"
#include "plotrenderer.h"
#include "plotstyle.h"
#include "renderframe.h"
#include "rtkplottypes.h"
#include "trackindex.h"
#include "tracklod.h"
#include "trackrendermodel.h"
#include "trackseries.h"

namespace {

constexpr int kWidth = 1200;
constexpr int kHeight = 800;

double medianOf(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

QVector<TrajectoryPoint> makeSyntheticTrack(int count)
{
    QVector<TrajectoryPoint> points;
    points.reserve(count);

    std::mt19937 rng(20240218);
    std::normal_distribution<double> step(0.8, 0.35);
    double x = 0.0;
    double y = 0.0;

    for (int i = 0; i < count; ++i) {
        x += step(rng) + 0.15;
        y += step(rng) - 0.05;

        TrajectoryPoint point;
        point.x = x;
        point.y = y;
        point.z = 2.0 * std::sin(i * 0.001);
        point.time = 1000.0 + i * 0.1;
        point.quality = (i % 97 == 0) ? 2 : 1;
        point.type = point.quality == 1 ? 0 : 1;
        point.sdx = 0.01;
        point.sdy = 0.01;
        points.append(point);
    }
    return points;
}

int framePointCount(const RenderFrame &frame)
{
    int total = 0;
    for (const QPolygonF &polyline : frame.polylines) {
        total += polyline.size();
    }
    return total;
}

void appendResult(const QString &name, double ms, int points)
{
    QTextStream out(stdout);
    out << QStringLiteral("%1,%2,%3\n").arg(name).arg(ms, 0, 'f', 3).arg(points);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);

    const int count = argc > 1 ? QString::fromLocal8Bit(argv[1]).toInt() : 200000;
    const QString parseFile = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();

    out << QStringLiteral("case,ms,points\n");

    // 1. 数据装载 + 空间索引
    const QVector<TrajectoryPoint> track = makeSyntheticTrack(count);
    TrackSeries series;
    TrackIndex index;

    QElapsedTimer timer;
    timer.start();
    series.setPoints(track);
    index.rebuild(series);
    appendResult(QStringLiteral("build_index"), timer.nsecsElapsed() / 1e6, count);

    const QSize viewport(kWidth, kHeight);
    const Camera2D fitCamera = Camera2D::fit(series.bounds(), viewport);

    LodPolicy lod;
    lod.maxPoints = 10000;

    RenderOptions options;
    options.showMarkers = true;
    options.highQuality = true;

    // 2. 全图建帧
    timer.restart();
    const RenderFrame overview = TrackRenderModel::build(series, index, fitCamera, lod, options);
    appendResult(QStringLiteral("frame_overview"), timer.nsecsElapsed() / 1e6, framePointCount(overview));

    // 3. 放大 100 倍建帧
    Camera2D zoomCamera = fitCamera;
    zoomCamera.setScale(fitCamera.scale() * 100.0);
    timer.restart();
    const RenderFrame zoomed = TrackRenderModel::build(series, index, zoomCamera, lod, options);
    appendResult(QStringLiteral("frame_zoom_100x"), timer.nsecsElapsed() / 1e6, framePointCount(zoomed));

    // 4. 渲染（离屏 QImage，20 次取中位数）
    PlotStyle style;
    style.showGrid = true;
    style.showTrack = true;
    style.showMarkers = true;
    style.markerSize = 3.0;
    style.lineWidth = 1.0;
    style.renderLevel = 1;

    QImage image(kWidth, kHeight, QImage::Format_ARGB32_Premultiplied);
    QVector<double> samples;
    for (int i = 0; i < 20; ++i) {
        timer.restart();
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        PlotRenderer::paintGrid(painter, fitCamera, style);
        PlotRenderer::paintFrame(painter, overview, style);
        PlotRenderer::paintAxes(painter, fitCamera, style);
        PlotRenderer::paintScale(painter, fitCamera, style);
        painter.end();
        samples.append(timer.nsecsElapsed() / 1e6);
    }
    appendResult(QStringLiteral("render_frame"), medianOf(samples), count);

    // 5. 可选：真实文件解析吞吐
    if (!parseFile.isEmpty()) {
        timer.restart();
        const FileParser::Result result = FileParser::parse(parseFile);
        const double parseMs = timer.nsecsElapsed() / 1e6;
        appendResult(QStringLiteral("parse_file"), parseMs, result.epochs);
        out << QStringLiteral("# parsed %1 (%2 bytes) -> %3 epochs, %4 ms\n")
                   .arg(QFileInfo(parseFile).fileName())
                   .arg(QFileInfo(parseFile).size())
                   .arg(result.epochs)
                   .arg(parseMs, 0, 'f', 1);
    }

    out.flush();
    return 0;
}
