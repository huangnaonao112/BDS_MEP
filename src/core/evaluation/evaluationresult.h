#ifndef EVALUATIONRESULT_H
#define EVALUATIONRESULT_H

#include <QString>
#include <QStringList>
#include <QVector>

// 评估结果模型。
//
// 旧版 EvaluationResult 是 60 多个平铺字段（baselineXxx、evaluationXxx、
// fixedPlane1sigma ...），报告代码只能靠复制粘贴拼字符串。这里按语义分组：
//   FileSummary      单个文件的质量分布
//   ErrorStats       一类样本（固定解/浮点解/全部）的精度指标
//   ThresholdTable   误差分布表（自带分母）
// 报告、txt 结果文件、测试都直接遍历这些结构，不再数下标。
namespace ev {

struct QualityCounts {
    int total = 0;
    int fix = 0;
    int floatCount = 0;
    int dgps = 0;    // 含 SBAS
    int single = 0;
    int ppp = 0;
    int other = 0;

    void add(int quality);
    double percent(int count) const;
};

struct ErrorStats {
    int sampleCount = 0;

    double rmsPlane = 0.0;
    double rmsHeight = 0.0;
    double meanPlane = 0.0;
    double meanHeight = 0.0;

    // 标准差（总体，1σ），与"68.27% 分位"是两回事，报告里分开标注
    double stdPlane = 0.0;
    double stdHeight = 0.0;

    double medianPlane = 0.0;   // CEP50
    double medianHeight = 0.0;
    double cep95Plane = 0.0;
    double cep95Height = 0.0;

    // 正态分位（旧版误称为 1σ/2σ/3σ）
    double p68Plane = 0.0, p95Plane = 0.0, p99Plane = 0.0;
    double p68Height = 0.0, p95Height = 0.0, p99Height = 0.0;

    double maxPlane = 0.0;
    double maxHeight = 0.0;
};

struct ThresholdTable {
    QVector<double> thresholds;  // 升序
    QVector<int> counts;         // counts[i] = 误差 < thresholds[i] 的样本数
    int sampleCount = 0;         // 分母

    void configure(const QVector<double> &values);
    void compute(const QVector<double> &values);
    double percent(int index) const;
};

struct FileSummary {
    QString fileName;
    QString format;
    QString timeRange;
    bool hasDate = false;
    int epochCount = 0;
    double spanSeconds = 0.0;
    double intervalSeconds = 0.0;
    QualityCounts quality;
};

struct ComparisonSample {
    double time = 0.0;
    double baselineE = 0.0, baselineN = 0.0, baselineU = 0.0;
    double baselineQuality = 0.0;
    double evalE = 0.0, evalN = 0.0, evalU = 0.0;
    double evalQuality = 0.0;
    double dx = 0.0, dy = 0.0, dz = 0.0;
    double planeError = 0.0;
    double heightError = 0.0;
};

struct EvaluationResult {
    bool valid = false;

    FileSummary baseline;
    FileSummary evaluation;

    int matchedCount = 0;
    int matchedBaselineFix = 0;
    int matchedBaselineFloat = 0;
    int matchedBaselineSingle = 0;
    int matchedBaselineDgps = 0;
    int matchedBaselineOther = 0;

    int matchedFixFix = 0;
    int matchedFixFloat = 0;
    int matchedFixSingle = 0;

    bool alignedByTimeOfDay = false;   // 一方带日期、一方只有时刻
    bool differentTimeBases = false;
    double timeTolerance = 0.1;
    QString alignmentNote;

    ErrorStats fixFix;        // 基准固定解 ∩ 评估固定解
    ErrorStats fixFloat;      // 基准固定解 ∩ 评估浮点解
    ErrorStats baselineFix;   // 基准固定解（不论评估解状态）——"全部数据"
    ErrorStats baselineFloat; // 基准浮点解

    ThresholdTable planeTable;   // 分母 = matchedBaselineFix
    ThresholdTable heightTable;  // 分母 = matchedBaselineFix

    int availabilityNumerator = 0;
    int availabilityDenominator = 0;
    double continuousAvailability = 0.0;
    double initializationTime = -1.0;  // 秒；无固定解时 -1

    QStringList warnings;
    QStringList resultFiles;
};

} // namespace ev

#endif // EVALUATIONRESULT_H
