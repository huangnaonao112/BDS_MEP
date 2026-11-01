#include "reportbuilder.h"

#include "gnssdefs.h"

#include <QStringList>

namespace {

using ev::EvaluationResult;
using ev::ErrorStats;
using ev::QualityCounts;
using ev::ThresholdTable;

QString esc(const QString &text)
{
    return text.toHtmlEscaped();
}

QString pct(int part, int total)
{
    if (total <= 0) {
        return QStringLiteral("0.00%");
    }
    return QString::number(static_cast<double>(part) / total * 100.0, 'f', 2) + QLatin1Char('%');
}

QString num(double value, int decimals = 3)
{
    return QString::number(value, 'f', decimals);
}

struct QualityRow {
    QString name;
    int count;
};

QVector<QualityRow> qualityRows(const QualityCounts &counts)
{
    return {
        {QStringLiteral("固定解"), counts.fix},
        {QStringLiteral("浮点解"), counts.floatCount},
        {QStringLiteral("差分解"), counts.dgps},
        {QStringLiteral("单点解"), counts.single},
        {QStringLiteral("PPP/其它"), counts.ppp + counts.other},
    };
}

void appendQualityText(QStringList &out, const QString &title, const QualityCounts &counts)
{
    out << QStringLiteral("  %1（共 %2 条）").arg(title).arg(counts.total);
    for (const QualityRow &row : qualityRows(counts)) {
        out << QStringLiteral("    %1: %2 条 (%3)")
                   .arg(row.name, QString::number(row.count), pct(row.count, counts.total));
    }
}

void appendDistributionText(QStringList &out, const QString &title, const ThresholdTable &table)
{
    out << QStringLiteral("  %1（分母：%2 个基准固定解匹配历元）").arg(title).arg(table.sampleCount);
    for (int i = table.thresholds.size() - 1; i >= 0; --i) {
        out << QStringLiteral("    < %1 m: %2 条 (%3)")
                   .arg(num(table.thresholds[i], 2),
                        QString::number(table.counts[i]),
                        pct(table.counts[i], table.sampleCount));
    }
}

void appendStatsText(QStringList &out, const QString &title, const ErrorStats &stats)
{
    out << QStringLiteral("  [%1] 样本 %2").arg(title).arg(stats.sampleCount);
    out << QStringLiteral("    平面  RMS %1  CEP50 %2  95% %3  σ %4  68.27%分位 %5  最大 %6  (m)")
               .arg(num(stats.rmsPlane), num(stats.medianPlane), num(stats.cep95Plane),
                    num(stats.stdPlane), num(stats.p68Plane), num(stats.maxPlane));
    out << QStringLiteral("    高程  RMS %1  CEP50 %2  95% %3  σ %4  68.27%分位 %5  最大 %6  (m)")
               .arg(num(stats.rmsHeight), num(stats.medianHeight), num(stats.cep95Height),
                    num(stats.stdHeight), num(stats.p68Height), num(stats.maxHeight));
}

QString htmlQualityTable(const QualityCounts &counts)
{
    QString html = QStringLiteral("<table><tr><th>解状态</th><th>数量</th><th>占比</th></tr>\n");
    for (const QualityRow &row : qualityRows(counts)) {
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>\n")
                    .arg(row.name, QString::number(row.count), pct(row.count, counts.total));
    }
    html += QStringLiteral("<tr><td><strong>合计</strong></td><td><strong>%1</strong></td>"
                           "<td><strong>100.00%</strong></td></tr>\n</table>\n")
                .arg(counts.total);
    return html;
}

QString htmlDistributionTable(const ThresholdTable &table)
{
    QString html = QStringLiteral("<table><tr><th>误差区间</th><th>数量</th><th>占比</th></tr>\n");
    for (int i = table.thresholds.size() - 1; i >= 0; --i) {
        html += QStringLiteral("<tr><td>&lt; %1 m</td><td>%2</td><td>%3</td></tr>\n")
                    .arg(num(table.thresholds[i], 2),
                         QString::number(table.counts[i]),
                         pct(table.counts[i], table.sampleCount));
    }
    html += QStringLiteral("<tr><td colspan=\"3\" class=\"muted\">分母：%1 个基准固定解匹配历元</td></tr>\n</table>\n")
                .arg(table.sampleCount);
    return html;
}

QString htmlStatsRow(const QString &title, const ErrorStats &stats)
{
    const QString plane = QStringLiteral("RMS %1 / CEP50 %2 / 95% %3 / σ %4 / P68 %5")
                              .arg(num(stats.rmsPlane), num(stats.medianPlane), num(stats.cep95Plane),
                                   num(stats.stdPlane), num(stats.p68Plane));
    const QString height = QStringLiteral("RMS %1 / CEP50 %2 / 95% %3 / σ %4 / P68 %5")
                               .arg(num(stats.rmsHeight), num(stats.medianHeight), num(stats.cep95Height),
                                    num(stats.stdHeight), num(stats.p68Height));
    return QStringLiteral("<tr><th>%1</th><td>%2</td><td>%3</td></tr>\n")
        .arg(title, plane, height);
}

} // namespace

QString ReportBuilder::number(double value, int decimals)
{
    return num(value, decimals);
}

QString ReportBuilder::percent(int part, int total)
{
    return pct(part, total);
}

QString ReportBuilder::metricsLine(const ErrorStats &stats)
{
    return QStringLiteral("样本 %1 | 平面 RMS %2 CEP50 %3 95% %4 σ %5 | 高程 RMS %6 CEP50 %7 95% %8 σ %9 (m)")
        .arg(stats.sampleCount)
        .arg(num(stats.rmsPlane), num(stats.medianPlane), num(stats.cep95Plane), num(stats.stdPlane),
             num(stats.rmsHeight), num(stats.medianHeight), num(stats.cep95Height), num(stats.stdHeight));
}

QString ReportBuilder::buildText(const EvaluationResult &r, const QDateTime &generatedAt)
{
    QStringList out;
    out << QStringLiteral("================ GNSS 数据评估报告 ================");
    out << QStringLiteral("生成时间: %1").arg(generatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    out << QString();

    out << QStringLiteral("一、基准文件");
    out << QStringLiteral("  文件名  : %1").arg(r.baseline.fileName);
    out << QStringLiteral("  格式    : %1").arg(r.baseline.format);
    out << QStringLiteral("  时间范围: %1").arg(r.baseline.timeRange);
    out << QStringLiteral("  有效历元: %1").arg(r.baseline.epochCount);
    out << QStringLiteral("  采样间隔: %1 s").arg(num(r.baseline.intervalSeconds, 2));
    appendQualityText(out, QStringLiteral("解状态分布"), r.baseline.quality);
    out << QString();

    out << QStringLiteral("二、评估文件");
    out << QStringLiteral("  文件名  : %1").arg(r.evaluation.fileName);
    out << QStringLiteral("  格式    : %1").arg(r.evaluation.format);
    out << QStringLiteral("  时间范围: %1").arg(r.evaluation.timeRange);
    out << QStringLiteral("  有效历元: %1").arg(r.evaluation.epochCount);
    out << QStringLiteral("  采样间隔: %1 s").arg(num(r.evaluation.intervalSeconds, 2));
    appendQualityText(out, QStringLiteral("解状态分布"), r.evaluation.quality);
    out << QString();

    out << QStringLiteral("三、时间对齐");
    out << QStringLiteral("  匹配容差: ±%1 s").arg(num(r.timeTolerance, 3));
    out << QStringLiteral("  匹配历元: %1").arg(r.matchedCount);
    if (r.differentTimeBases) {
        out << QStringLiteral("  时间基准: 两侧不完全一致（一方不含日期），已按参照文件补全日期");
        if (!r.alignmentNote.isEmpty()) {
            out << QStringLiteral("            %1").arg(r.alignmentNote);
        }
    } else {
        out << QStringLiteral("  时间基准: 两侧均含日期，按绝对时间对齐");
    }
    out << QString();

    out << QStringLiteral("四、比较结果");
    out << QStringLiteral("  基准固定解匹配      : %1").arg(r.matchedBaselineFix);
    out << QStringLiteral("  其中 固定-固定      : %1").arg(r.matchedFixFix);
    out << QStringLiteral("  其中 固定-浮点      : %1").arg(r.matchedFixFloat);
    out << QStringLiteral("  其中 固定-其它解状态: %1").arg(r.matchedFixSingle);
    out << QStringLiteral("  基准浮点解匹配      : %1").arg(r.matchedBaselineFloat);
    out << QStringLiteral("  基准单点解匹配      : %1").arg(r.matchedBaselineSingle);
    out << QStringLiteral("  基准差分解匹配      : %1").arg(r.matchedBaselineDgps);
    out << QString();

    out << QStringLiteral("五、误差分布");
    appendDistributionText(out, QStringLiteral("平面误差"), r.planeTable);
    appendDistributionText(out, QStringLiteral("高程误差"), r.heightTable);
    out << QString();

    out << QStringLiteral("六、精度指标");
    appendStatsText(out, QStringLiteral("固定解-固定解"), r.fixFix);
    appendStatsText(out, QStringLiteral("固定解-浮点解"), r.fixFloat);
    appendStatsText(out, QStringLiteral("基准固定解-全部评估点"), r.baselineFix);
    appendStatsText(out, QStringLiteral("基准浮点解"), r.baselineFloat);
    out << QString();

    out << QStringLiteral("七、可用性");
    out << QStringLiteral("  连续可用性: %1  (%2 / %3 个评估历元满足 平面<5cm 且 高程<8cm)")
               .arg(pct(r.availabilityNumerator, r.availabilityDenominator))
               .arg(r.availabilityNumerator)
               .arg(r.availabilityDenominator);
    out << QStringLiteral("  初始化时间: %1")
               .arg(r.initializationTime >= 0.0
                        ? QStringLiteral("%1 s").arg(num(r.initializationTime, 1))
                        : QStringLiteral("N/A（评估文件无固定解）"));
    out << QString();

    if (!r.warnings.isEmpty()) {
        out << QStringLiteral("八、提示");
        for (const QString &warning : r.warnings) {
            out << QStringLiteral("  - %1").arg(warning);
        }
    } else {
        out << QStringLiteral("八、提示");
        out << QStringLiteral("  - 无异常提示");
    }
    out << QString();

    out << QStringLiteral("九、统计口径");
    out << QStringLiteral("  1. 误差分布的分母是“基准固定解匹配历元数”（%1），不是全部匹配历元。")
               .arg(r.matchedBaselineFix);
    out << QStringLiteral("     基准里混有浮点解/单点解时，用全部匹配点数做分母会系统性低估占比。");
    out << QStringLiteral("  2. σ 为总体标准差；P68/P95/P99 是 68.27%/95.45%/99.73% 分位点。");
    out << QStringLiteral("     两者不是一回事，报告中分开列出。");
    out << QStringLiteral("  3. 平面误差 = ENU 水平残差模长；高程误差 = |ΔUp|。");
    out << QStringLiteral("  4. 时间以文件记录值为准，不做 GPST/UTC 互转；跨格式对齐请确认两侧时间基准一致。");
    out << QStringLiteral("  5. 表格源没有解状态字段，按固定解参与统计（解析日志中会提示）。");
    out << QString();

    out << QStringLiteral("================ 评估结束 ================");

    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString ReportBuilder::buildHtml(const EvaluationResult &r, const QDateTime &generatedAt)
{
    QString html;
    html += QStringLiteral("<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"utf-8\">\n");
    html += QStringLiteral("<title>GNSS 数据评估报告</title>\n<style>\n");
    html += QStringLiteral(
        "body{font-family:'Microsoft YaHei',Arial,sans-serif;margin:0;background:#eef1f5;color:#222;}\n"
        ".report{max-width:1200px;margin:24px auto;background:#fff;padding:32px;border-radius:8px;"
        "box-shadow:0 2px 12px rgba(0,0,0,.08);}\n"
        "h1{font-size:24px;text-align:center;border-bottom:2px solid #2f6fb5;padding-bottom:12px;}\n"
        "h2{font-size:17px;margin-top:28px;border-left:4px solid #2f6fb5;padding-left:10px;}\n"
        "table{border-collapse:collapse;width:100%;margin:12px 0;font-size:13px;}\n"
        "th,td{border:1px solid #dfe3e8;padding:8px 10px;text-align:center;}\n"
        "th{background:#2f6fb5;color:#fff;font-weight:600;}\n"
        "tr:nth-child(even) td{background:#f8fafc;}\n"
        ".kv{width:100%;font-size:13px;}\n"
        ".kv th{background:#f2f5f8;color:#333;text-align:right;width:150px;}\n"
        ".kv td{text-align:left;}\n"
        ".muted{color:#888;font-size:12px;}\n"
        ".note{background:#f7f9fc;border:1px solid #e2e8f0;border-radius:6px;padding:12px 16px;font-size:13px;}\n"
        ".warn{background:#fff8e6;border:1px solid #f0d18a;border-radius:6px;padding:10px 16px;font-size:13px;}\n"
        ".footer{margin-top:32px;text-align:center;color:#888;font-size:12px;}\n");
    html += QStringLiteral("</style>\n</head>\n<body>\n<div class=\"report\">\n");

    html += QStringLiteral("<h1>GNSS 数据评估报告</h1>\n");

    // 1. 基准
    html += QStringLiteral("<h2>1. 基准文件</h2>\n<table class=\"kv\">\n");
    html += QStringLiteral("<tr><th>文件名</th><td>%1</td></tr>\n").arg(esc(r.baseline.fileName));
    html += QStringLiteral("<tr><th>格式</th><td>%1</td></tr>\n").arg(esc(r.baseline.format));
    html += QStringLiteral("<tr><th>时间范围</th><td>%1</td></tr>\n").arg(esc(r.baseline.timeRange));
    html += QStringLiteral("<tr><th>有效历元</th><td>%1</td></tr>\n").arg(r.baseline.epochCount);
    html += QStringLiteral("<tr><th>采样间隔</th><td>%1 s</td></tr>\n").arg(num(r.baseline.intervalSeconds, 2));
    html += QStringLiteral("</table>\n");
    html += QStringLiteral("<h2>2. 基准文件解状态分布</h2>\n");
    html += htmlQualityTable(r.baseline.quality);

    // 2. 评估
    html += QStringLiteral("<h2>3. 评估文件</h2>\n<table class=\"kv\">\n");
    html += QStringLiteral("<tr><th>文件名</th><td>%1</td></tr>\n").arg(esc(r.evaluation.fileName));
    html += QStringLiteral("<tr><th>格式</th><td>%1</td></tr>\n").arg(esc(r.evaluation.format));
    html += QStringLiteral("<tr><th>时间范围</th><td>%1</td></tr>\n").arg(esc(r.evaluation.timeRange));
    html += QStringLiteral("<tr><th>有效历元</th><td>%1</td></tr>\n").arg(r.evaluation.epochCount);
    html += QStringLiteral("<tr><th>采样间隔</th><td>%1 s</td></tr>\n").arg(num(r.evaluation.intervalSeconds, 2));
    html += QStringLiteral("</table>\n");
    html += QStringLiteral("<h2>4. 评估文件解状态分布</h2>\n");
    html += htmlQualityTable(r.evaluation.quality);

    // 3. 时间对齐与匹配
    html += QStringLiteral("<h2>5. 时间对齐与比较</h2>\n<table class=\"kv\">\n");
    html += QStringLiteral("<tr><th>匹配容差</th><td>±%1 s</td></tr>\n").arg(num(r.timeTolerance, 3));
    html += QStringLiteral("<tr><th>匹配历元</th><td>%1</td></tr>\n").arg(r.matchedCount);
    html += QStringLiteral("<tr><th>基准固定解匹配</th><td>%1</td></tr>\n").arg(r.matchedBaselineFix);
    html += QStringLiteral("<tr><th>固定-固定 / 固定-浮点 / 固定-其它</th><td>%1 / %2 / %3</td></tr>\n")
                .arg(r.matchedFixFix).arg(r.matchedFixFloat).arg(r.matchedFixSingle);
    html += QStringLiteral("<tr><th>时间基准</th><td>%1</td></tr>\n")
                .arg(r.differentTimeBases
                         ? esc(r.alignmentNote.isEmpty()
                                   ? QStringLiteral("两侧时间基准不一致，已补全日期")
                                   : r.alignmentNote)
                         : QStringLiteral("两侧均含日期，按绝对时间对齐"));
    html += QStringLiteral("</table>\n");

    // 4. 误差分布
    html += QStringLiteral("<h2>6. 平面误差分布</h2>\n");
    html += htmlDistributionTable(r.planeTable);
    html += QStringLiteral("<h2>7. 高程误差分布</h2>\n");
    html += htmlDistributionTable(r.heightTable);

    // 5. 精度
    html += QStringLiteral("<h2>8. 精度指标</h2>\n<table>\n");
    html += QStringLiteral("<tr><th>样本类别</th><th>平面 (m)</th><th>高程 (m)</th></tr>\n");
    html += htmlStatsRow(QStringLiteral("固定解-固定解"), r.fixFix);
    html += htmlStatsRow(QStringLiteral("固定解-浮点解"), r.fixFloat);
    html += htmlStatsRow(QStringLiteral("基准固定解-全部评估点"), r.baselineFix);
    html += htmlStatsRow(QStringLiteral("基准浮点解"), r.baselineFloat);
    html += QStringLiteral("</table>\n");
    html += QStringLiteral("<p class=\"muted\">RMS=均方根；CEP50=中位数；95%=95% 分位；σ=总体标准差；"
                           "P68=68.27% 分位。σ 与 P68 不是同一概念。</p>\n");

    // 6. 可用性
    html += QStringLiteral("<h2>9. 可用性</h2>\n<table class=\"kv\">\n");
    html += QStringLiteral("<tr><th>连续可用性</th><td>%1（%2 / %3 历元满足 平面&lt;5cm 且 高程&lt;8cm）</td></tr>\n")
                .arg(pct(r.availabilityNumerator, r.availabilityDenominator))
                .arg(r.availabilityNumerator)
                .arg(r.availabilityDenominator);
    html += QStringLiteral("<tr><th>初始化时间</th><td>%1</td></tr>\n")
                .arg(r.initializationTime >= 0.0
                         ? QStringLiteral("%1 s").arg(num(r.initializationTime, 1))
                         : QStringLiteral("N/A（评估文件无固定解）"));
    html += QStringLiteral("</table>\n");

    if (!r.warnings.isEmpty()) {
        html += QStringLiteral("<h2>10. 提示</h2>\n<div class=\"warn\">\n<ul>\n");
        for (const QString &warning : r.warnings) {
            html += QStringLiteral("<li>%1</li>\n").arg(esc(warning));
        }
        html += QStringLiteral("</ul>\n</div>\n");
    }

    html += QStringLiteral("<h2>统计口径</h2>\n<div class=\"note\">\n<ol>\n");
    html += QStringLiteral("<li>误差分布分母为基准固定解匹配历元数（%1），不是全部匹配历元。</li>\n")
                .arg(r.matchedBaselineFix);
    html += QStringLiteral("<li>σ 为总体标准差；P68/P95/P99 为 68.27%/95.45%/99.73% 分位点，两者分列。</li>\n");
    html += QStringLiteral("<li>平面误差 = ENU 水平残差模长；高程误差 = |ΔUp|。</li>\n");
    html += QStringLiteral("<li>时间以文件记录值为准，不做 GPST/UTC 互转。</li>\n");
    html += QStringLiteral("<li>表格源无解状态字段，按固定解参与统计。</li>\n");
    html += QStringLiteral("</ol>\n</div>\n");

    html += QStringLiteral("<div class=\"footer\">生成时间：%1</div>\n")
                .arg(generatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    html += QStringLiteral("</div>\n</body>\n</html>\n");

    return html;
}
