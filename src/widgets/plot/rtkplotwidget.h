#ifndef RTKPLOTWIDGET_H
#define RTKPLOTWIDGET_H

#include <QElapsedTimer>
#include <QPixmap>
#include <QPoint>
#include <QTimer>
#include <QWidget>

#include "camera2d.h"
#include "plotstyle.h"
#include "renderframe.h"
#include "rtkplottypes.h"
#include "trackindex.h"
#include "tracklod.h"
#include "trackseries.h"

// 轨迹图控件：只做输入 → 相机、缓存调度、把渲染帧交给 PlotRenderer。
// 几何可见性在 TrackRenderModel，绘制在 PlotRenderer，控件本身不碰数据过滤。
class RTKPlotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RTKPlotWidget(QWidget *parent = nullptr);
    ~RTKPlotWidget() override;

    // 数据
    void clearData();
    void addTrajectoryPoint(const TrajectoryPoint &point);
    void setTrajectoryData(const QVector<TrajectoryPoint> &points);
    int dataPointCount() const { return m_series.size(); }

    // 显示选项
    void setShowTrack(bool show);
    void setShowVelocity(bool show);
    void setShowErrorCircles(bool show);
    void setShowArrows(bool show);
    void setTrackColor(const QColor &color);
    void setLineWidth(double width);

    // 视图
    void setCenter(double x, double y);
    void zoomToFit();
    void setZoom(double factor);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void setZoomRange(double minZoom, double maxZoom);

    PlotType plotType() const { return m_plotType; }
    Camera2D camera() const { return m_camera; }

signals:
    void pointSelected(const TrajectoryPoint &point);
    void plotTypeChanged(PlotType type);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void invalidateCache();
    void syncIndex();
    Camera2D renderCamera() const;
    PlotStyle currentStyle() const;
    void rebuildCache();
    void startAnimation(const Camera2D &target);
    void updateAnimation();
    static double clampScale(double scale, double minScale, double maxScale);

    PlotType m_plotType = PLOT_TRK;

    TrackSeries m_series;
    TrackIndex m_index;
    Camera2D m_camera;

    Camera2D m_animFrom;
    Camera2D m_animTo;
    QElapsedTimer m_animClock;
    int m_animDurationMs = 180;
    bool m_animating = false;
    QTimer *m_animationTimer = nullptr;

    PlotStyle m_style;
    double m_minZoom = 1e-9;
    double m_maxZoom = 1e9;
    bool m_autoFit = true;
    int m_renderLevel = 1;

    bool m_dragging = false;
    QPoint m_lastMousePos;

    QPixmap m_cache;
    Camera2D m_cacheCamera;
    quint64 m_cacheRevision = 0;
    QSize m_cacheSize;
    bool m_cacheValid = false;
};

#endif // RTKPLOTWIDGET_H
