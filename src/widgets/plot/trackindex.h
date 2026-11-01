#ifndef PLOT_TRACKINDEX_H
#define PLOT_TRACKINDEX_H

#include <QHash>
#include <QRectF>
#include <QVector>

#include "trackseries.h"

// 连续的一段数据索引，闭区间 [first, last]。
struct IndexRun {
    int first = 0;
    int last = 0;
    int length() const { return last - first + 1; }
};

// 轨迹空间索引。
//
// 职责只有一件事：给定世界矩形，返回**索引区间**（而不是"可见点列表"）。
// 区间两侧各多带一个数据点，保证折线能越过视口边缘、交给裁剪层切齐。
// 视口内一个点都没有时，退化为"线段-矩形求交"，把横穿视口的线段也找出来。
class TrackIndex
{
public:
    void rebuild(const TrackSeries &series);
    void clear();

    bool isEmpty() const { return m_points.isEmpty(); }
    quint64 revision() const { return m_revision; }

    QVector<IndexRun> queryVisible(const QRectF &worldRect) const;

private:
    QVector<int> collectCandidates(const QRectF &rect, bool precise, bool *tooMany) const;
    QVector<IndexRun> buildRuns(QVector<int> indices) const;

    // 找到"横穿视口但两端都在视口外"的线段端点：
    // 按最大线段长度扩张查询范围，保证相交线段至少有一个端点落在扩张区内。
    QVector<int> crossingIndices(const QRectF &rect) const;

    quint64 gridKey(int gx, int gy) const;
    void cellOf(double x, double y, int *gx, int *gy) const;

    QVector<TrajectoryPoint> m_points;   // 与 TrackSeries 共享底层数据（COW）
    QRectF m_bounds;
    int m_gridSize = 1;
    double m_cellWidth = 1.0;
    double m_cellHeight = 1.0;
    QHash<quint64, QVector<int>> m_grid;
    double m_maxSegmentLength = 0.0;   // 相邻点最大间距，用于扩张"线段求交"查询范围
    quint64 m_revision = 0;
};

#endif // PLOT_TRACKINDEX_H
