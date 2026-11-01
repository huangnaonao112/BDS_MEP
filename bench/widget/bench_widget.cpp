#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QMouseEvent>
#include <QTextStream>
#include <QVector>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <random>

#include "rtkplottypes.h"
#include "rtkplotwidget.h"

namespace {

// 统计真实 paint 事件节奏（而不是我们自己调 render() 的频率）
class PaintCounter : public QObject
{
public:
    QVector<qint64> arrivals;
    QElapsedTimer clock;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (event->type() == QEvent::Paint) {
            arrivals.append(clock.elapsed());
        }
        return false;
    }
};

QVector<TrajectoryPoint> makeTrack(int count)
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
        points.append(point);
    }
    return points;
}

double median(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QMouseEvent makeMouseEvent(QEvent::Type type, const QPoint &pos, Qt::MouseButton button,
                           Qt::MouseButtons buttons)
{
    return QMouseEvent(type, QPointF(pos), QPointF(pos), button, buttons, Qt::NoModifier);
}
#else
QMouseEvent makeMouseEvent(QEvent::Type type, const QPoint &pos, Qt::MouseButton button,
                           Qt::MouseButtons buttons)
{
    return QMouseEvent(type, QPointF(pos), button, buttons, Qt::NoModifier);
}
#endif

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const int count = argc > 1 ? QString::fromLocal8Bit(argv[1]).toInt() : 200000;

    RTKPlotWidget plot;
    plot.resize(1200, 800);
    plot.show();
    plot.setTrajectoryData(makeTrack(count));
    QApplication::processEvents();

    QImage image(1200, 800, QImage::Format_ARGB32_Premultiplied);

    // 首次渲染：完整质量（含标记点、标签、抗锯齿）
    QElapsedTimer timer;
    timer.start();
    plot.render(&image);
    const double firstFrame = timer.nsecsElapsed() / 1e6;

    // 缓存命中时的重绘
    timer.restart();
    plot.render(&image);
    const double cachedFrame = timer.nsecsElapsed() / 1e6;

    // 拖动：交互质量，逐帧重建
    const QPoint pos(600, 400);
    QMouseEvent press = makeMouseEvent(QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton);
    QApplication::sendEvent(&plot, &press);

    QVector<double> drag;
    for (int i = 0; i < 60; ++i) {
        const QPoint p = pos + QPoint(i, i / 2);
        QMouseEvent move = makeMouseEvent(QEvent::MouseMove, p, Qt::NoButton, Qt::LeftButton);
        QApplication::sendEvent(&plot, &move);

        timer.restart();
        plot.render(&image);
        drag.append(timer.nsecsElapsed() / 1e6);
    }

    QMouseEvent release = makeMouseEvent(QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton);
    QApplication::sendEvent(&plot, &release);

    // 松手后的第一帧：恢复完整质量
    timer.restart();
    plot.render(&image);
    const double settleFrame = timer.nsecsElapsed() / 1e6;

    // 滚轮缩放动画的真实出帧节奏
    PaintCounter counter;
    plot.installEventFilter(&counter);
    counter.clock.start();

    QWheelEvent wheel(QPointF(600, 400), QPointF(700, 500), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&plot, &wheel);

    while (counter.clock.elapsed() < 500) {
        QApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    plot.removeEventFilter(&counter);

    QVector<double> intervals;
    for (int i = 1; i < counter.arrivals.size(); ++i) {
        intervals.append(static_cast<double>(counter.arrivals[i] - counter.arrivals[i - 1]));
    }
    const double medianInterval = median(intervals);

    QTextStream out(stdout);
    out << QStringLiteral("case,ms,points\n");
    out << QStringLiteral("first_frame,%1,%2\n").arg(firstFrame, 0, 'f', 3).arg(count);
    out << QStringLiteral("cached_frame,%1,%2\n").arg(cachedFrame, 0, 'f', 3).arg(count);
    out << QStringLiteral("drag_frame,%1,%2\n").arg(median(drag), 0, 'f', 3).arg(count);
    out << QStringLiteral("settle_frame,%1,%2\n").arg(settleFrame, 0, 'f', 3).arg(count);
    out << QStringLiteral("anim_paint_interval,%1,%2\n").arg(medianInterval, 0, 'f', 3).arg(count);
    out << QStringLiteral("anim_paints,%1,%2\n").arg(counter.arrivals.size()).arg(count);
    out.flush();
    return 0;
}
