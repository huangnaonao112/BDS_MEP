#include "errorstats.h"

#include <algorithm>
#include <cmath>

namespace ev {

double percentile(const QVector<double> &sorted, double p)
{
    const int n = sorted.size();
    if (n <= 0) {
        return 0.0;
    }
    if (n == 1) {
        return sorted.first();
    }
    if (p <= 0.0) {
        return sorted.first();
    }
    if (p >= 1.0) {
        return sorted.last();
    }

    const double position = p * (n - 1);
    const int lower = static_cast<int>(std::floor(position));
    const int upper = std::min(lower + 1, n - 1);
    const double fraction = position - lower;
    return sorted[lower] + (sorted[upper] - sorted[lower]) * fraction;
}

namespace {

void fillAxis(const QVector<double> &values, ErrorStats *stats, bool plane)
{
    QVector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    const int n = sorted.size();
    double sum = 0.0;
    double sumSquares = 0.0;
    for (double value : sorted) {
        sum += value;
        sumSquares += value * value;
    }

    const double mean = n > 0 ? sum / n : 0.0;
    const double rms = n > 0 ? std::sqrt(sumSquares / n) : 0.0;

    double variance = 0.0;
    for (double value : sorted) {
        const double diff = value - mean;
        variance += diff * diff;
    }
    variance = n > 1 ? variance / n : 0.0;   // 总体方差（GNSS 惯例）

    if (plane) {
        stats->meanPlane = mean;
        stats->rmsPlane = rms;
        stats->stdPlane = std::sqrt(variance);
        stats->medianPlane = percentile(sorted, 0.50);
        stats->cep95Plane = percentile(sorted, 0.95);
        stats->p68Plane = percentile(sorted, 0.6827);
        stats->p95Plane = percentile(sorted, 0.9545);
        stats->p99Plane = percentile(sorted, 0.9973);
        stats->maxPlane = n > 0 ? sorted.last() : 0.0;
    } else {
        stats->meanHeight = mean;
        stats->rmsHeight = rms;
        stats->stdHeight = std::sqrt(variance);
        stats->medianHeight = percentile(sorted, 0.50);
        stats->cep95Height = percentile(sorted, 0.95);
        stats->p68Height = percentile(sorted, 0.6827);
        stats->p95Height = percentile(sorted, 0.9545);
        stats->p99Height = percentile(sorted, 0.9973);
        stats->maxHeight = n > 0 ? sorted.last() : 0.0;
    }
}

} // namespace

ErrorStats computeStats(const QVector<double> &planeErrors,
                        const QVector<double> &heightErrors)
{
    ErrorStats stats;
    stats.sampleCount = std::min(planeErrors.size(), heightErrors.size());
    if (stats.sampleCount <= 0) {
        return stats;
    }

    fillAxis(planeErrors, &stats, true);
    fillAxis(heightErrors, &stats, false);
    return stats;
}

} // namespace ev
