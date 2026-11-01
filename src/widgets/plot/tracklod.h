#ifndef PLOT_TRACKLOD_H
#define PLOT_TRACKLOD_H

#include <QVector>

#include "trackindex.h"
#include "trackseries.h"

// 抽稀策略：只决定"画多少个点"，不决定"哪些几何存在"。
// 每个连续索引段的首尾一定保留（它们是越过视口边缘的锚点）。
struct LodPolicy {
    int maxPoints = 8000;
};

class TrackLod
{
public:
    static QVector<int> select(const TrackSeries &series,
                               const QVector<IndexRun> &runs,
                               const LodPolicy &policy);
};

#endif // PLOT_TRACKLOD_H
