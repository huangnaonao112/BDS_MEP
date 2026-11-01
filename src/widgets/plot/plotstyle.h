#ifndef PLOT_PLOTSTYLE_H
#define PLOT_PLOTSTYLE_H

#include <QColor>

// 绘图外观。颜色/线宽/开关只影响"怎么画"，不参与"画什么"的几何计算。
struct PlotStyle {
    QColor background = Qt::white;
    QColor gridColor = Qt::lightGray;
    QColor textColor = Qt::black;
    QColor trackColor = Qt::blue;

    double lineWidth = 1.0;

    bool showTrack = true;
    bool showMarkers = true;
    bool showVelocity = false;
    bool showErrorCircles = false;
    bool showArrows = false;
    bool showGrid = true;
    bool showScale = true;

    // 标记直径（像素），由 widget 按当前缩放算好（对数增长，避免高倍率下看不见）
    double markerSize = 3.0;

    int renderLevel = 1;   // 0=拖拽/动画低质量，1=高质量

    bool highQuality() const { return renderLevel > 0; }
};

#endif // PLOT_PLOTSTYLE_H
