#ifndef EVALUATIONENGINE_H
#define EVALUATIONENGINE_H

#include <functional>

#include "epochdata.h"
#include "evaluationresult.h"

// 双文件时间对齐 + 精度统计。纯计算，不读文件、不写文件、不碰 UI。
namespace ev {

struct EvaluationOptions {
    // 历元匹配容差（秒）。1 Hz 数据两侧对齐后误差通常 < 0.01 s。
    double matchTolerance = 0.1;

    // 评估前抽稀间隔（秒），<=0 表示不抽稀
    double thinInterval = 1.0;

    // 连续可用性判据：平面 < 5 cm 且高程 < 8 cm
    double availabilityPlaneLimit = 0.05;
    double availabilityHeightLimit = 0.08;
};

using SampleSink = std::function<void(const ComparisonSample &)>;

// 对齐规则：
//   * 两侧都有日期 -> 直接比较绝对时间；
//   * 一侧只有时刻（NMEA GGA）-> 以另一侧首个历元的日期为基准补全日期。
//     （旧实现把日期丢掉后在各处 ±86400/fmod 打补丁，跨零点必错。）
EvaluationResult evaluate(const gnss::EpochSeries &baselineSeries,
                          const gnss::EpochSeries &evaluationSeries,
                          const EvaluationOptions &options,
                          const SampleSink &sink = SampleSink());

} // namespace ev

#endif // EVALUATIONENGINE_H
