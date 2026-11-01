#include "trackindex.h"

#include <algorithm>
#include <cmath>

namespace {

// Liang-Barsky：线段是否与矩形相交（含端点在内部/边界的情况）
bool segmentIntersectsRect(const QPointF &a, const QPointF &b, const QRectF &rect)
{
    double t0 = 0.0;
    double t1 = 1.0;
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();

    const double p[4] = {-dx, dx, -dy, dy};
    const double q[4] = {a.x() - rect.left(), rect.right() - a.x(),
                         a.y() - rect.top(), rect.bottom() - a.y()};

    for (int i = 0; i < 4; ++i) {
        if (std::fabs(p[i]) < 1e-12) {
            if (q[i] < 0.0) {
                return false;
            }
            continue;
        }
        const double r = q[i] / p[i];
        if (p[i] < 0.0) {
            if (r > t1) return false;
            if (r > t0) t0 = r;
        } else {
            if (r < t0) return false;
            if (r < t1) t1 = r;
        }
    }
    return true;
}

} // namespace

void TrackIndex::clear()
{
    m_points.clear();
    m_bounds = QRectF();
    m_grid.clear();
    m_gridSize = 1;
    m_revision = 0;
}

quint64 TrackIndex::gridKey(int gx, int gy) const
{
    return (static_cast<quint64>(gx) << 32) | static_cast<quint32>(gy);
}

void TrackIndex::cellOf(double x, double y, int *gx, int *gy) const
{
    *gx = m_cellWidth > 1e-12 ? static_cast<int>((x - m_bounds.left()) / m_cellWidth) : 0;
    *gy = m_cellHeight > 1e-12 ? static_cast<int>((y - m_bounds.top()) / m_cellHeight) : 0;
    *gx = std::clamp(*gx, 0, m_gridSize - 1);
    *gy = std::clamp(*gy, 0, m_gridSize - 1);
}

void TrackIndex::rebuild(const TrackSeries &series)
{
    clear();
    m_points = series.points();   // QVector 隐式共享，不复制数据
    m_bounds = series.bounds();
    m_revision = series.revision();

    const int n = m_points.size();
    if (n == 0) {
        return;
    }

    m_gridSize = std::clamp(static_cast<int>(std::sqrt(static_cast<double>(n))), 20, 1000);
    const double rangeX = std::max(m_bounds.width(), 1e-9);
    const double rangeY = std::max(m_bounds.height(), 1e-9);
    m_cellWidth = rangeX / m_gridSize;
    m_cellHeight = rangeY / m_gridSize;

    m_grid.reserve(std::min(n, m_gridSize * m_gridSize / 2));

    for (int i = 0; i < n; ++i) {
        int gx = 0;
        int gy = 0;
        cellOf(m_points[i].x, m_points[i].y, &gx, &gy);
        m_grid[gridKey(gx, gy)].append(i);

        if (i > 0) {
            const double dx = m_points[i].x - m_points[i - 1].x;
            const double dy = m_points[i].y - m_points[i - 1].y;
            m_maxSegmentLength = std::max(m_maxSegmentLength, std::sqrt(dx * dx + dy * dy));
        }
    }
}

QVector<int> TrackIndex::collectCandidates(const QRectF &rect, bool precise, bool *tooMany) const
{
    QVector<int> candidates;
    if (tooMany) {
        *tooMany = false;
    }
    if (m_points.isEmpty()) {
        return candidates;
    }

    int gx1 = 0, gy1 = 0, gx2 = 0, gy2 = 0;
    cellOf(rect.left(), rect.top(), &gx1, &gy1);
    cellOf(rect.right(), rect.bottom(), &gx2, &gy2);

    const int cellsX = std::abs(gx2 - gx1) + 1;
    const int cellsY = std::abs(gy2 - gy1) + 1;
    if (static_cast<qint64>(cellsX) * cellsY > 20000) {
        if (tooMany) {
            *tooMany = true;
        }
        return candidates;
    }

    const int x0 = std::min(gx1, gx2);
    const int x1 = std::max(gx1, gx2);
    const int y0 = std::min(gy1, gy2);
    const int y1 = std::max(gy1, gy2);
    candidates.reserve(std::min(m_points.size(), (x1 - x0 + 1) * (y1 - y0 + 1) * 8));

    for (int gx = x0; gx <= x1; ++gx) {
        for (int gy = y0; gy <= y1; ++gy) {
            const auto it = m_grid.constFind(gridKey(gx, gy));
            if (it == m_grid.constEnd()) {
                continue;
            }
            for (int index : it.value()) {
                if (precise) {
                    const TrajectoryPoint &p = m_points[index];
                    if (!rect.contains(QPointF(p.x, p.y))) {
                        continue;
                    }
                }
                candidates.append(index);
            }
        }
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates;
}

QVector<IndexRun> TrackIndex::buildRuns(QVector<int> indices) const
{
    QVector<IndexRun> runs;
    if (indices.isEmpty()) {
        return runs;
    }

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    const int n = m_points.size();
    int start = indices.first();
    int previous = start;

    const auto appendRun = [&](int first, int last) {
        // 两侧各扩一个点：让折线越过视口边缘，由渲染层裁齐
        IndexRun run;
        run.first = std::max(0, first - 1);
        run.last = std::min(n - 1, last + 1);
        if (!runs.isEmpty() && run.first <= runs.last().last + 1) {
            runs.last().last = std::max(runs.last().last, run.last);
        } else {
            runs.append(run);
        }
    };

    for (int i = 1; i < indices.size(); ++i) {
        if (indices[i] == previous + 1) {
            previous = indices[i];
            continue;
        }
        appendRun(start, previous);
        start = indices[i];
        previous = start;
    }
    appendRun(start, previous);
    return runs;
}

QVector<int> TrackIndex::crossingIndices(const QRectF &rect) const
{
    // 相交线段至少有一个端点距离矩形不超过线段长度 L，因此把查询范围
    // 各方向扩张 L，扩张区内的相邻点对再做一次精确的线段-矩形求交。
    QVector<int> crossing;
    if (m_maxSegmentLength <= 0.0 || m_points.size() < 2) {
        return crossing;
    }

    const double pad = m_maxSegmentLength * 1.05;
    const QRectF expanded = rect.adjusted(-pad, -pad, pad, pad);

    bool tooMany = false;
    const QVector<int> candidates = collectCandidates(expanded, false, &tooMany);
    // 候选过多时放弃这条兜底路径（正常缩放几乎不会触发）
    if (tooMany || candidates.size() < 2 || candidates.size() > 200000) {
        return crossing;
    }

    // 候选点只保证"至少一个端点在扩张区内"，所以要对每个候选点
    // 与它在数据上的前后邻居各做一次求交，才能覆盖整条横穿线段。
    const int n = m_points.size();
    for (int index : candidates) {
        const TrajectoryPoint &current = m_points[index];

        if (index > 0) {
            const TrajectoryPoint &previous = m_points[index - 1];
            const QRectF box(QPointF(std::min(previous.x, current.x), std::min(previous.y, current.y)),
                             QPointF(std::max(previous.x, current.x), std::max(previous.y, current.y)));
            if (box.adjusted(-1e-12, -1e-12, 1e-12, 1e-12).intersects(rect)
                && segmentIntersectsRect(QPointF(previous.x, previous.y),
                                         QPointF(current.x, current.y), rect)) {
                crossing.append(index - 1);
                crossing.append(index);
            }
        }
        if (index + 1 < n) {
            const TrajectoryPoint &next = m_points[index + 1];
            const QRectF box(QPointF(std::min(current.x, next.x), std::min(current.y, next.y)),
                             QPointF(std::max(current.x, next.x), std::max(current.y, next.y)));
            if (box.adjusted(-1e-12, -1e-12, 1e-12, 1e-12).intersects(rect)
                && segmentIntersectsRect(QPointF(current.x, current.y),
                                         QPointF(next.x, next.y), rect)) {
                crossing.append(index);
                crossing.append(index + 1);
            }
        }
    }
    return crossing;
}

QVector<IndexRun> TrackIndex::queryVisible(const QRectF &worldRect) const
{
    if (m_points.isEmpty() || worldRect.isNull()) {
        return {};
    }

    // 全图：直接返回整个序列，由 LOD 决定画多少
    if (worldRect.contains(m_bounds)) {
        return {IndexRun{0, m_points.size() - 1}};
    }

    bool tooMany = false;
    QVector<int> inside = collectCandidates(worldRect, true, &tooMany);
    if (tooMany) {
        // 视口覆盖了过多网格单元：退化为全量 + LOD
        return {IndexRun{0, m_points.size() - 1}};
    }

    // 补上"两端都在视口外、但线段横穿视口"的部分（含视口内一个点都没有的情况）
    const QVector<int> crossing = crossingIndices(worldRect);
    inside << crossing;

    if (inside.isEmpty()) {
        return {};
    }

    return buildRuns(inside);
}
