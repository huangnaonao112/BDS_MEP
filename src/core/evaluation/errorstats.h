#ifndef ERRORSTATS_H
#define ERRORSTATS_H

#include <QVector>

#include "evaluationresult.h"

// 纯函数统计工具，独立于解析/UI，便于单元测试。
namespace ev {

// 线性插值分位数，sorted 必须已升序，p ∈ [0,1]
double percentile(const QVector<double> &sorted, double p);

// 由平面/高程误差样本计算整套指标（RMS、均值、标准差、中位数、95%、分位、极值）
ErrorStats computeStats(const QVector<double> &planeErrors,
                        const QVector<double> &heightErrors);

} // namespace ev

#endif // ERRORSTATS_H
