#include "rtkplotwidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "plotrenderer.h"
#include "trackrendermodel.h"

namespace {

QPoint mousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

} // namespace

RTKPlotWidget::RTKPlotWidget(QWidget *parent)
    : QWidget(parent)
{
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(16);
    m_animationTimer->setTimerType(Qt::PreciseTimer);
    connect(m_animationTimer, &QTimer::timeout, this, &RTKPlotWidget::updateAnimation);

    // 自己铺满背景，省掉每次 paint 前的二次填充
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

RTKPlotWidget::~RTKPlotWidget() = default;

void RTKPlotWidget::clearData()
{
    m_series.clear();
    m_index.clear();
    m_animationTimer->stop();
    m_animating = false;
    m_autoFit = true;
    invalidateCache();
    update();
}

void RTKPlotWidget::addTrajectoryPoint(const TrajectoryPoint &point)
{
    m_series.append(point);
    invalidateCache();
    update();
}

void RTKPlotWidget::setTrajectoryData(const QVector<TrajectoryPoint> &points)
{
    m_series.setPoints(points);
    syncIndex();
    m_autoFit = true;
    zoomToFit();
}

void RTKPlotWidget::setShowTrack(bool show)
{
    m_style.showTrack = show;
    invalidateCache();
    update();
}

void RTKPlotWidget::setShowVelocity(bool show)
{
    m_style.showVelocity = show;
    invalidateCache();
    update();
}

void RTKPlotWidget::setShowErrorCircles(bool show)
{
    m_style.showErrorCircles = show;
    invalidateCache();
    update();
}

void RTKPlotWidget::setShowArrows(bool show)
{
    m_style.showArrows = show;
    invalidateCache();
    update();
}

void RTKPlotWidget::setTrackColor(const QColor &color)
{
    m_style.trackColor = color;
    invalidateCache();
    update();
}

void RTKPlotWidget::setLineWidth(double width)
{
    m_style.lineWidth = width;
    invalidateCache();
    update();
}

double RTKPlotWidget::clampScale(double scale, double minScale, double maxScale)
{
    return std::clamp(scale, minScale, maxScale);
}

void RTKPlotWidget::setCenter(double x, double y)
{
    m_camera = renderCamera();
    m_animationTimer->stop();
    m_animating = false;
    m_camera.setCenter(QPointF(x, y));
    m_autoFit = false;
    invalidateCache();
    update();
}

void RTKPlotWidget::zoomToFit()
{
    if (!size().isValid()) {
        return;
    }

    m_animationTimer->stop();
    m_animating = false;
    m_camera.setViewport(size());

    if (m_series.isEmpty()) {
        m_camera.setCenter(QPointF(0.0, 0.0));
        m_camera.setScale(100.0);
    } else {
        m_camera = Camera2D::fit(m_series.bounds(), size(), 10.0);
        m_camera.setScale(clampScale(m_camera.scale(), m_minZoom, m_maxZoom));
        m_camera.setViewport(size());
    }

    m_autoFit = true;
    invalidateCache();
    update();
}

void RTKPlotWidget::setZoom(double factor)
{
    m_camera = renderCamera();
    m_animationTimer->stop();
    m_animating = false;
    m_camera.setScale(clampScale(factor, m_minZoom, m_maxZoom));
    m_autoFit = false;
    invalidateCache();
    update();
}

void RTKPlotWidget::zoomIn()
{
    setZoom(renderCamera().scale() * 1.2);
}

void RTKPlotWidget::zoomOut()
{
    setZoom(renderCamera().scale() / 1.2);
}

void RTKPlotWidget::resetZoom()
{
    m_autoFit = true;
    zoomToFit();
}

void RTKPlotWidget::setZoomRange(double minZoom, double maxZoom)
{
    m_minZoom = minZoom;
    m_maxZoom = maxZoom;
    if (m_camera.scale() < m_minZoom || m_camera.scale() > m_maxZoom) {
        m_camera.setScale(clampScale(m_camera.scale(), m_minZoom, m_maxZoom));
        invalidateCache();
    }
}

void RTKPlotWidget::syncIndex()
{
    if (m_index.revision() != m_series.revision()) {
        m_index.rebuild(m_series);
    }
}

Camera2D RTKPlotWidget::renderCamera() const
{
    if (!m_animating) {
        return m_camera;
    }

    // 时间驱动：帧率低时动画不会变慢，只会少几帧
    const double t = std::clamp(static_cast<double>(m_animClock.elapsed()) / m_animDurationMs, 0.0, 1.0);
    const double eased = 1.0 - std::pow(1.0 - t, 3.0);   // ease-out cubic
    return Camera2D::interpolate(m_animFrom, m_animTo, eased);
}

PlotStyle RTKPlotWidget::currentStyle() const
{
    PlotStyle style = m_style;
    const bool interacting = m_animating || m_dragging;
    style.renderLevel = interacting ? 0 : m_renderLevel;

    const double scale = std::max(1.0, renderCamera().scale());
    // 交互期间标记点降到低质量预算（1000 个），松手后恢复完整数量（约 5000 个）。
    // 完全不画会让用户觉得"点消失了"，降到 1/5 既省时又保持轨迹形态。
    style.showMarkers = style.highQuality() || scale > 10.0;

    const double base = style.highQuality() ? 3.0 : 2.0;
    const double logZoom = std::log10(scale);
    style.markerSize = std::clamp(base + 0.5 * logZoom, base, 10.0);
    return style;
}

void RTKPlotWidget::invalidateCache()
{
    m_cacheValid = false;
}

void RTKPlotWidget::rebuildCache()
{
    if (width() <= 0 || height() <= 0) {
        return;
    }

    syncIndex();

    const Camera2D camera = renderCamera();
    const PlotStyle style = currentStyle();

    LodPolicy lod;
    // 交互期间不降到 1000 点：1000 点的折线会明显"棱角化"，用户感知为不丝滑。
    // 4000 点的绘制成本仍远低于 16 ms 帧预算。
    lod.maxPoints = style.highQuality() ? 10000 : 4000;

    RenderOptions options;
    options.showMarkers = style.showMarkers;
    options.showErrorCircles = style.showErrorCircles;
    options.showArrows = style.showArrows;
    options.highQuality = style.highQuality();

    const RenderFrame frame = TrackRenderModel::build(m_series, m_index, camera, lod, options);

    // 复用 pixmap：每帧重新分配一张 1200x800 的 QPixmap 在拖动时会明显掉帧
    if (m_cache.size() != size()) {
        m_cache = QPixmap(size());
    }
    m_cache.fill(style.background);

    QPainter painter(&m_cache);
    // 网格/坐标轴用硬边（便宜），轨迹线始终开抗锯齿——交互时不牺牲线条观感
    painter.setRenderHint(QPainter::Antialiasing, false);

    if (style.showGrid) {
        PlotRenderer::paintGrid(painter, camera, style);
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    PlotRenderer::paintFrame(painter, frame, style);
    painter.setRenderHint(QPainter::Antialiasing, style.highQuality());
    PlotRenderer::paintAxes(painter, camera, style);
    if (style.showScale) {
        PlotRenderer::paintScale(painter, camera, style);
    }
    painter.end();

    m_cacheCamera = camera;
    m_cacheRevision = m_series.revision();
    m_cacheSize = size();
    m_cacheValid = true;
}

void RTKPlotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (width() <= 0 || height() <= 0) {
        return;
    }

    if (m_camera.viewport() != size()) {
        m_camera.setViewport(size());
        if (m_autoFit) {
            zoomToFit();
        }
    }

    const Camera2D camera = renderCamera();
    if (!m_cacheValid || m_cacheSize != size() || m_cacheRevision != m_series.revision()
        || m_cacheCamera != camera) {
        rebuildCache();
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_cache);
}

void RTKPlotWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    m_camera.setViewport(size());
    m_animationTimer->stop();
    m_animating = false;

    if (m_autoFit) {
        zoomToFit();
    } else {
        invalidateCache();
        update();
    }
}

void RTKPlotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    m_camera = renderCamera();
    m_animationTimer->stop();
    m_animating = false;
    m_dragging = true;
    m_autoFit = false;
    m_lastMousePos = mousePosition(event);
    setCursor(Qt::ClosedHandCursor);
}

void RTKPlotWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        return;
    }

    const QPoint delta = mousePosition(event) - m_lastMousePos;
    m_lastMousePos = mousePosition(event);
    m_camera.panByPixels(QPointF(delta));
    invalidateCache();

    // 不再手工节流：update() 本身会合并，手工 16 ms 门限反而会丢掉少量移动事件，
    // 表现为拖动时轻微一顿一顿。
    update();
}

void RTKPlotWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    m_dragging = false;
    setCursor(Qt::ArrowCursor);
    invalidateCache();
    update();
}

void RTKPlotWidget::wheelEvent(QWheelEvent *event)
{
    // 不丢弃高频滚轮事件（触控板常以 >60 Hz 发事件），节流会让缩放一顿一顿。
    // 连续滚动时在动画目标上累加，手感是"连续放大"而不是"每次从头动画"。
    const Camera2D base = m_animating ? m_animTo : m_camera;
    const double factor = std::pow(1.2, event->angleDelta().y() / 120.0);
    const double newScale = clampScale(base.scale() * factor, m_minZoom, m_maxZoom);
    if (newScale == base.scale()) {
        return;
    }

    Camera2D target = base;
    target.zoomAt(event->position(), newScale / base.scale());
    m_autoFit = false;

    if (m_animating && target == m_animTo) {
        return;
    }
    startAnimation(target);
}

void RTKPlotWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        break;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        zoomOut();
        break;
    case Qt::Key_Home:
        resetZoom();
        break;
    case Qt::Key_F:
        zoomToFit();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void RTKPlotWidget::startAnimation(const Camera2D &target)
{
    m_animFrom = renderCamera();
    m_animTo = target;
    m_animClock.start();
    m_animating = true;
    m_animationTimer->start();
    invalidateCache();
    update();
}

void RTKPlotWidget::updateAnimation()
{
    if (!m_animating) {
        return;
    }

    if (m_animClock.elapsed() >= m_animDurationMs) {
        m_camera = m_animTo;
        m_animating = false;
        m_animationTimer->stop();
    }

    invalidateCache();
    update();
}
