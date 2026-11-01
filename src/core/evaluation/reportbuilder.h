#ifndef REPORTBUILDER_H
#define REPORTBUILDER_H

#include <QDateTime>
#include <QString>

#include "evaluationresult.h"

// 报告生成：文本 + HTML。
//
// 旧实现是 56 处 report += 和 111 处 html += 的硬编码拼接，改一个字段要
// 同步改十处。这里把"布局"和"数据"分开：数据来自 EvaluationResult，
// 布局由少量模板函数 + 循环生成，新增指标只需改一处。
class ReportBuilder
{
public:
    static QString buildText(const ev::EvaluationResult &result, const QDateTime &generatedAt);
    static QString buildHtml(const ev::EvaluationResult &result, const QDateTime &generatedAt);

    // 单行指标（供 txt 结果文件复用）："平面 RMS=... CEP50=... 95%=... σ=..."
    static QString metricsLine(const ev::ErrorStats &stats);

private:
    static QString number(double value, int decimals = 3);
    static QString percent(int part, int total);
};

#endif // REPORTBUILDER_H
