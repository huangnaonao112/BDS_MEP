#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

#include "evaluationpipeline.h"

// 无界面评估入口：给 CI、批处理和性能测试用。
// 用法：
//   bdsmep-cli -b base.pos -e rover.nmea -o out/
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("bdsmep-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("GNSS 数据评估（命令行）"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption baselineOption(
        {QStringLiteral("b"), QStringLiteral("baseline")},
        QStringLiteral("基准文件 (.pos/.nmea/.txt)"), QStringLiteral("path"));
    const QCommandLineOption evaluationOption(
        {QStringLiteral("e"), QStringLiteral("evaluation")},
        QStringLiteral("评估文件 (.pos/.nmea/.txt)"), QStringLiteral("path"));
    const QCommandLineOption outputOption(
        {QStringLiteral("o"), QStringLiteral("output")},
        QStringLiteral("输出目录（默认当前目录）"), QStringLiteral("dir"),
        QStringLiteral("."));
    const QCommandLineOption toleranceOption(
        {QStringLiteral("t"), QStringLiteral("tolerance")},
        QStringLiteral("历元匹配容差（秒，默认 0.1）"), QStringLiteral("seconds"),
        QStringLiteral("0.1"));
    const QCommandLineOption thinOption(
        {QStringLiteral("s"), QStringLiteral("thin")},
        QStringLiteral("评估前抽稀间隔（秒，0 表示不抽稀，默认 1.0）"), QStringLiteral("seconds"),
        QStringLiteral("1.0"));

    parser.addOption(baselineOption);
    parser.addOption(evaluationOption);
    parser.addOption(outputOption);
    parser.addOption(toleranceOption);
    parser.addOption(thinOption);
    parser.process(app);

    if (!parser.isSet(baselineOption) || !parser.isSet(evaluationOption)) {
        QTextStream(stderr) << "缺少必要参数，使用 --help 查看用法\n";
        return 1;
    }

    ev::EvaluationRequest request;
    request.baselinePath = parser.value(baselineOption);
    request.evaluationPath = parser.value(evaluationOption);
    request.outputDirectory = parser.value(outputOption);
    request.options.matchTolerance = parser.value(toleranceOption).toDouble();
    request.options.thinInterval = parser.value(thinOption).toDouble();

    QTextStream out(stdout);
    ev::ProgressSink sink;
    sink.log = [&out](const QString &message, const QString &level) {
        out << QStringLiteral("[%1] %2\n").arg(level, message);
        out.flush();
    };

    const ev::EvaluationOutcome outcome = ev::runEvaluation(request, sink);
    if (!outcome.ok) {
        out << QStringLiteral("评估失败：%1\n").arg(outcome.error);
        return 1;
    }

    const ev::EvaluationResult &r = outcome.result;
    out << QStringLiteral("\n==== 摘要 ====\n");
    out << QStringLiteral("匹配历元      : %1\n").arg(r.matchedCount);
    out << QStringLiteral("基准固定解    : %1（固定-固定 %2）\n")
               .arg(r.matchedBaselineFix)
               .arg(r.matchedFixFix);
    out << QStringLiteral("固定-固定平面 : RMS %1  CEP50 %2  95% %3  σ %4 m\n")
               .arg(r.fixFix.rmsPlane, 0, 'f', 4)
               .arg(r.fixFix.medianPlane, 0, 'f', 4)
               .arg(r.fixFix.cep95Plane, 0, 'f', 4)
               .arg(r.fixFix.stdPlane, 0, 'f', 4);
    out << QStringLiteral("固定-固定高程 : RMS %1  CEP50 %2  95% %3  σ %4 m\n")
               .arg(r.fixFix.rmsHeight, 0, 'f', 4)
               .arg(r.fixFix.medianHeight, 0, 'f', 4)
               .arg(r.fixFix.cep95Height, 0, 'f', 4)
               .arg(r.fixFix.stdHeight, 0, 'f', 4);
    out << QStringLiteral("连续可用性    : %1% (%2/%3)\n")
               .arg(QString::number(r.continuousAvailability, 'f', 2))
               .arg(r.availabilityNumerator)
               .arg(r.availabilityDenominator);
    out << QStringLiteral("初始化时间    : %1\n")
               .arg(r.initializationTime >= 0.0
                        ? QStringLiteral("%1 s").arg(r.initializationTime, 0, 'f', 1)
                        : QStringLiteral("N/A"));

    for (const QString &warning : r.warnings) {
        out << QStringLiteral("提示: %1\n").arg(warning);
    }

    out << QStringLiteral("\n输出文件：\n");
    for (const QString &file : outcome.resultFiles) {
        out << QStringLiteral("  %1\n").arg(QFileInfo(file).fileName());
    }
    out.flush();

    return r.valid ? 0 : 2;
}
