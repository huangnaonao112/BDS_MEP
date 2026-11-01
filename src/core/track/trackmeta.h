#ifndef TRACKMETA_H
#define TRACKMETA_H

#include <QString>

// 文件元信息：任务详情表格、项目配置、日志都用这一套字段，
// 不再让 UI 自己在多处重复计算"开始时间/间隔/完整率"。
struct TrackMeta
{
    QString format;              // GGA / POS / TABULAR
    QString startTime;           // 文本形式，已按是否有日期格式化
    QString endTime;
    double intervalSeconds = 0;  // 中位数采样间隔
    int epochCount = 0;
    double completenessPercent = 100.0;
    double fixRatePercent = 0.0;
    double spanSeconds = 0;
};

#endif // TRACKMETA_H
