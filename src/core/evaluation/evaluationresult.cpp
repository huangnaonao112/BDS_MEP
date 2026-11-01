#include "evaluationresult.h"
#include "gnssdefs.h"

#include <algorithm>
#include <cmath>

namespace ev {

void QualityCounts::add(int quality)
{
    ++total;
    switch (quality) {
    case 1: ++fix; break;
    case 2: ++floatCount; break;
    case 3:
    case 4: ++dgps; break;
    case 5: ++single; break;
    case 6: ++ppp; break;
    default: ++other; break;
    }
}

double QualityCounts::percent(int count) const
{
    if (total <= 0) {
        return 0.0;
    }
    return static_cast<double>(count) / total * 100.0;
}

void ThresholdTable::configure(const QVector<double> &values)
{
    thresholds = values;
    counts.fill(0, values.size());
    sampleCount = 0;
}

void ThresholdTable::compute(const QVector<double> &values)
{
    counts.fill(0, thresholds.size());
    sampleCount = values.size();

    for (double value : values) {
        // 阈值升序：value 一旦小于 thresholds[i]，对所有更大的阈值也成立
        for (int i = 0; i < thresholds.size(); ++i) {
            if (value < thresholds[i]) {
                ++counts[i];
            }
        }
    }
}

double ThresholdTable::percent(int index) const
{
    if (sampleCount <= 0 || index < 0 || index >= counts.size()) {
        return 0.0;
    }
    return static_cast<double>(counts[index]) / sampleCount * 100.0;
}

} // namespace ev
