#include "evaluationengine.h"

#include "coordinates.h"
#include "errorstats.h"
#include "gnssdefs.h"
#include "timeutil.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace ev {

namespace {

const QVector<double> &planeThresholds()
{
    static const QVector<double> kThresholds = {0.02, 0.05, 0.10, 0.20, 0.50, 1.00};
    return kThresholds;
}

const QVector<double> &heightThresholds()
{
    static const QVector<double> kThresholds = {0.05, 0.10, 0.20, 0.50, 1.00, 2.00};
    return kThresholds;
}

FileSummary summarize(const gnss::EpochSeries &series)
{
    FileSummary summary;
    summary.format = series.format;
    summary.hasDate = series.hasDate;
    summary.epochCount = series.epochs.size();
    summary.spanSeconds = gnss::seriesSpanSeconds(series);
    summary.intervalSeconds = gnss::medianSampleInterval(series);
    summary.timeRange = series.epochs.isEmpty()
                            ? QStringLiteral("--")
                            : gnss::formatTimeRange(series.epochs.first().time,
                                                    series.epochs.last().time,
                                                    series.hasDate);

    for (const gnss::GnssEpoch &epoch : series.epochs) {
        summary.quality.add(epoch.quality);
    }
    return summary;
}

// 无日期一侧：按参考侧首个历元的日期补全，得到统一的绝对时间轴。
void attachDate(gnss::EpochSeries &series, double referenceStart)
{
    if (series.epochs.isEmpty()) {
        return;
    }
    const double offset = std::floor((referenceStart - series.epochs.first().time + gnss::kHalfDay)
                                     / gnss::kSecondsPerDay)
                          * gnss::kSecondsPerDay;
    for (gnss::GnssEpoch &epoch : series.epochs) {
        epoch.time += offset;
    }
    series.hasDate = true;
}

} // namespace

EvaluationResult evaluate(const gnss::EpochSeries &baselineSeries,
                          const gnss::EpochSeries &evaluationSeries,
                          const EvaluationOptions &options,
                          const SampleSink &sink)
{
    EvaluationResult result;
    result.timeTolerance = options.matchTolerance;

    gnss::EpochSeries baseline = baselineSeries;
    gnss::EpochSeries evaluation = evaluationSeries;

    if (options.thinInterval > 0.0) {
        baseline = gnss::thinSeries(baseline, options.thinInterval);
        evaluation = gnss::thinSeries(evaluation, options.thinInterval);
    }

    const QStringList baseNotes = gnss::normalizeSeries(baseline);
    const QStringList evalNotes = gnss::normalizeSeries(evaluation);
    result.warnings << baseNotes << evalNotes;

    result.baseline = summarize(baseline);
    result.evaluation = summarize(evaluation);

    if (baseline.epochs.isEmpty() || evaluation.epochs.isEmpty()) {
        result.warnings << QStringLiteral("基准文件或评估文件为空，无法评估");
        return result;
    }

    result.planeTable.configure(planeThresholds());
    result.heightTable.configure(heightThresholds());

    // ---- 时间基准统一 ----
    const bool baseHasDate = baseline.hasDate;
    const bool evalHasDate = evaluation.hasDate;
    result.alignedByTimeOfDay = baseHasDate != evalHasDate;
    result.differentTimeBases = baseHasDate != evalHasDate;

    if (!baseHasDate && evalHasDate) {
        attachDate(baseline, evaluation.epochs.first().time);
        result.alignmentNote = QStringLiteral("基准文件只有时刻，已按评估文件首个历元的日期补全");
    } else if (baseHasDate && !evalHasDate) {
        attachDate(evaluation, baseline.epochs.first().time);
        result.alignmentNote = QStringLiteral("评估文件只有时刻，已按基准文件首个历元的日期补全");
    }

    if (result.alignedByTimeOfDay) {
        result.baseline.timeRange = gnss::formatTimeRange(baseline.epochs.first().time,
                                                          baseline.epochs.last().time, false);
        result.evaluation.timeRange = gnss::formatTimeRange(evaluation.epochs.first().time,
                                                            evaluation.epochs.last().time, false);
    }

    // ---- 统一 ENU 坐标系：以基准文件首个历元的 BLH 为原点 ----
    gnss::GnssEpoch originEpoch = baseline.epochs.first();
    gnss::ensureBlh(originEpoch);
    if (!originEpoch.hasBlh) {
        result.warnings << QStringLiteral("基准文件首个历元缺少经纬度，无法建立 ENU 坐标系");
        return result;
    }
    const gnss::Blh origin{originEpoch.lat, originEpoch.lon, originEpoch.height};

    const int baseCount = baseline.epochs.size();
    const int evalCount = evaluation.epochs.size();

    QVector<gnss::Enu> baseEnu(baseCount);
    for (int i = 0; i < baseCount; ++i) {
        const gnss::GnssEpoch &epoch = baseline.epochs[i];
        baseEnu[i] = gnss::ecefToEnu(origin, gnss::Ecef{epoch.x, epoch.y, epoch.z});
    }
    QVector<gnss::Enu> evalEnu(evalCount);
    for (int j = 0; j < evalCount; ++j) {
        const gnss::GnssEpoch &epoch = evaluation.epochs[j];
        evalEnu[j] = gnss::ecefToEnu(origin, gnss::Ecef{epoch.x, epoch.y, epoch.z});
    }

    // ---- 双指针时间匹配 ----
    QVector<double> fixFixPlane, fixFixHeight;
    QVector<double> fixFloatPlane, fixFloatHeight;
    QVector<double> baseFixPlane, baseFixHeight;
    QVector<double> baseFloatPlane, baseFloatHeight;
    fixFixPlane.reserve(baseCount);
    fixFixHeight.reserve(baseCount);
    baseFixPlane.reserve(baseCount);
    baseFixHeight.reserve(baseCount);

    const double tolerance = std::max(1e-6, options.matchTolerance);

    int i = 0;
    int j = 0;
    while (i < baseCount && j < evalCount) {
        const double diff = baseline.epochs[i].time - evaluation.epochs[j].time;

        if (std::fabs(diff) <= tolerance) {
            const gnss::GnssEpoch &baseEpoch = baseline.epochs[i];
            const gnss::GnssEpoch &evalEpoch = evaluation.epochs[j];
            const gnss::Enu &basePoint = baseEnu[i];
            const gnss::Enu &evalPoint = evalEnu[j];

            const double dx = evalPoint.e - basePoint.e;
            const double dy = evalPoint.n - basePoint.n;
            const double dz = evalPoint.u - basePoint.u;

            ComparisonSample sample;
            sample.time = baseEpoch.time;
            sample.baselineE = basePoint.e;
            sample.baselineN = basePoint.n;
            sample.baselineU = basePoint.u;
            sample.baselineQuality = baseEpoch.quality;
            sample.evalE = evalPoint.e;
            sample.evalN = evalPoint.n;
            sample.evalU = evalPoint.u;
            sample.evalQuality = evalEpoch.quality;
            sample.dx = dx;
            sample.dy = dy;
            sample.dz = dz;
            sample.planeError = std::sqrt(dx * dx + dy * dy);
            sample.heightError = std::fabs(dz);

            ++result.matchedCount;

            const int baseQuality = baseEpoch.quality;
            const int evalQuality = evalEpoch.quality;

            if (gnss::isFix(baseQuality)) {
                ++result.matchedBaselineFix;
                baseFixPlane.append(sample.planeError);
                baseFixHeight.append(sample.heightError);

                if (gnss::isFix(evalQuality)) {
                    ++result.matchedFixFix;
                    fixFixPlane.append(sample.planeError);
                    fixFixHeight.append(sample.heightError);

                    if (sample.planeError < options.availabilityPlaneLimit
                        && sample.heightError < options.availabilityHeightLimit) {
                        ++result.availabilityNumerator;
                    }
                } else if (gnss::isFloat(evalQuality)) {
                    ++result.matchedFixFloat;
                    fixFloatPlane.append(sample.planeError);
                    fixFloatHeight.append(sample.heightError);
                } else {
                    ++result.matchedFixSingle;
                }
            } else if (gnss::isFloat(baseQuality)) {
                ++result.matchedBaselineFloat;
                baseFloatPlane.append(sample.planeError);
                baseFloatHeight.append(sample.heightError);
            } else if (baseQuality == 4 || baseQuality == 3) {
                ++result.matchedBaselineDgps;
            } else if (baseQuality == 5) {
                ++result.matchedBaselineSingle;
            } else {
                ++result.matchedBaselineOther;
            }

            if (sink) {
                sink(sample);
            }

            ++i;
            ++j;
        } else if (diff > 0.0) {
            ++j;  // 基准更晚：推进评估
        } else {
            ++i;  // 评估更晚：推进基准
        }
    }

    // ---- 指标汇总 ----
    result.fixFix = computeStats(fixFixPlane, fixFixHeight);
    result.fixFloat = computeStats(fixFloatPlane, fixFloatHeight);
    result.baselineFix = computeStats(baseFixPlane, baseFixHeight);
    result.baselineFloat = computeStats(baseFloatPlane, baseFloatHeight);

    // 误差分布的分母就是基准固定解样本数，不再是"全部匹配点"
    result.planeTable.compute(baseFixPlane);
    result.heightTable.compute(baseFixHeight);

    result.availabilityDenominator = evaluation.epochs.size();
    result.continuousAvailability = result.availabilityDenominator > 0
                                        ? static_cast<double>(result.availabilityNumerator)
                                              / result.availabilityDenominator * 100.0
                                        : 0.0;

    // 初始化时间：评估文件首个历元 -> 首个固定解
    result.initializationTime = -1.0;
    const double firstEvalTime = evaluation.epochs.first().time;
    for (const gnss::GnssEpoch &epoch : evaluation.epochs) {
        if (gnss::isFix(epoch.quality)) {
            result.initializationTime = epoch.time - firstEvalTime;
            break;
        }
    }

    // ---- 诊断信息 ----
    if (result.matchedCount == 0) {
        result.warnings << QStringLiteral("两个文件没有可匹配的历元：请检查时间基准、采样时刻与文件是否同名同段");
    } else {
        const int smaller = std::min(baseCount, evalCount);
        const double matchRate = smaller > 0 ? static_cast<double>(result.matchedCount) / smaller * 100.0 : 0.0;
        if (matchRate < 50.0) {
            result.warnings << QStringLiteral("匹配率仅 %1%，可能存在时间基准差异或采样率不一致")
                                   .arg(QString::number(matchRate, 'f', 1));
        }
    }

    if (result.baseline.quality.fix == 0) {
        result.warnings << QStringLiteral("基准文件没有固定解历元，固定解精度与误差分布没有样本");
    }
    if (result.initializationTime < 0.0) {
        result.warnings << QStringLiteral("评估文件没有任何固定解历元，初始化时间记为 N/A");
    }

    result.valid = result.matchedCount > 0;
    return result;
}

} // namespace ev
