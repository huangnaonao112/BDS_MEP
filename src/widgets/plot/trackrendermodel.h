#ifndef PLOT_TRACKRENDERMODEL_H
#define PLOT_TRACKRENDERMODEL_H

#include "camera2d.h"
#include "renderframe.h"
#include "trackindex.h"
#include "tracklod.h"
#include "trackseries.h"

// 几何构建选项：决定一帧里要不要附加误差圆、速度箭头等装饰
struct RenderOptions {
    bool showMarkers = true;
    bool showErrorCircles = false;
    bool showArrows = false;
    bool highQuality = true;
};

// 可见性 + 抽稀 + 坐标变换。
//
// 这是唯一知道"什么该出现在画布上"的地方，契约是：
//   任何与 camera.visibleWorld() 相交的线段，都必须出现在输出帧的折线里
//   （端点允许落在视口外，渲染层用 clipRect 裁齐）。
class TrackRenderModel
{
public:
    static RenderFrame build(const TrackSeries &series,
                             const TrackIndex &index,
                             const Camera2D &camera,
                             const LodPolicy &lod,
                             const RenderOptions &options);
};

#endif // PLOT_TRACKRENDERMODEL_H
