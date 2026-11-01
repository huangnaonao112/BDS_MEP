#include "tracklod.h"

#include <algorithm>
#include <cmath>

QVector<int> TrackLod::select(const TrackSeries &series,
                              const QVector<IndexRun> &runs,
                              const LodPolicy &policy)
{
    QVector<int> result;
    if (runs.isEmpty() || series.isEmpty()) {
        return result;
    }

    const int budget = std::max(2, policy.maxPoints);
    int total = 0;
    for (const IndexRun &run : runs) {
        total += run.length();
    }

    if (total <= budget) {
        result.reserve(total);
        for (const IndexRun &run : runs) {
            for (int i = run.first; i <= run.last; ++i) {
                result.append(i);
            }
        }
        return result;
    }

    result.reserve(budget + runs.size() * 2);

    for (const IndexRun &run : runs) {
        const int length = run.length();
        // 按长度分配预算，每段至少 2 个点
        const int share = std::max(2, static_cast<int>(std::llround(
                                          static_cast<double>(budget) * length / total)));
        const int step = std::max(1, static_cast<int>(std::ceil(
                                         static_cast<double>(length) / share)));

        result.append(run.first);
        for (int i = run.first + step; i < run.last; i += step) {
            result.append(i);
        }
        result.append(run.last);   // 尾部锚点必须保留
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
