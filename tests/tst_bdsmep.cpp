#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include "coordinates.h"
#include "epochdata.h"
#include "evaluationengine.h"
#include "evaluationresult.h"
#include "errorstats.h"
#include "fileparser.h"
#include "gnssdefs.h"
#include "gnssparser.h"
#include "projectmanager.h"
#include "timeutil.h"
#include "trajectorydata.h"
#include "textfilereader.h"

#include <cmath>

namespace {

QString writeTextFile(const QString &directory, const QString &name, const QString &content)
{
    const QString path = QDir(directory).filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream out(&file);
#if !defined(QT6)
    out.setCodec("UTF-8");
#endif
    out << content;
    file.close();
    return path;
}

// 在赤道/本初子午线建立测试点：east=+y, north=+z, up=+x
gnss::GnssEpoch makeEpoch(double time, int quality, double east = 0.0,
                          double up = 0.0, bool = true)
{
    gnss::GnssEpoch epoch;
    epoch.time = time;
    epoch.quality = quality;
    epoch.lat = 0.0;
    epoch.lon = 0.0;
    epoch.height = 0.0;
    epoch.hasBlh = true;
    epoch.x = gnss::kA + up;
    epoch.y = east;
    epoch.z = 0.0;
    epoch.hasEcef = true;
    return epoch;
}

gnss::EpochSeries makeSeries(const QVector<gnss::GnssEpoch> &epochs, bool hasDate)
{
    gnss::EpochSeries series;
    series.epochs = epochs;
    series.hasDate = hasDate;
    series.format = QStringLiteral("TEST");
    return series;
}

} // namespace

class TestGnssCore : public QObject
{
    Q_OBJECT

private slots:
    void coordinatesRoundTrip();
    void enuOffsetsMapToExpectedAxes();
    void qualityNormalization();
    void ggaParserReadsPositionAndQuality();
    void ggaParserRejectsInvalidQuality();
    void posParserKeepsDateAcrossMidnight();
    void posParserWithoutDateFallsBackToTimeOfDay();
    void textReaderDetectsGbkEncoding();
    void normalizeUnwrapsBeforeSorting();
    void engineAlignsAcrossMidnight();
    void engineAttachesDateToTimeOfDaySeries();
    void distributionDenominatorIsBaselineFix();
    void percentileDiffersFromStandardDeviation();
    void initializationTimeUsesFirstFix();
    void toleranceRejectsMismatchedEpochs();
    void trajectoryCompletenessReflectsGap();
    void projectRoundTrip();
};

void TestGnssCore::coordinatesRoundTrip()
{
    const gnss::Blh blh{31.9250000, 118.5250000, 58.3};
    const gnss::Ecef ecef = gnss::blhToEcef(blh);
    const gnss::Blh back = gnss::ecefToBlh(ecef);

    QVERIFY(std::fabs(back.lat - blh.lat) < 1e-9);
    QVERIFY(std::fabs(back.lon - blh.lon) < 1e-9);
    QVERIFY(std::fabs(back.height - blh.height) < 1e-4);

    // 已知量级校验：赤道上经度 1° ≈ 111.32 km（a·sin1° = 111313.84 m）
    const gnss::Ecef east1 = gnss::blhToEcef(gnss::Blh{0.0, 1.0, 0.0});
    QVERIFY2(std::fabs(east1.y - 111313.839) < 1.0,
             qPrintable(QStringLiteral("y=%1 x=%2 z=%3")
                            .arg(east1.y, 0, 'f', 3)
                            .arg(east1.x, 0, 'f', 3)
                            .arg(east1.z, 0, 'f', 3)));
}

void TestGnssCore::enuOffsetsMapToExpectedAxes()
{
    const gnss::Blh origin{0.0, 0.0, 0.0};
    const gnss::Ecef point{gnss::kA, 3.0, 4.0};   // east=3, north=4, up=0
    const gnss::Enu enu = gnss::ecefToEnu(origin, point);

    QVERIFY(std::fabs(enu.e - 3.0) < 1e-6);
    QVERIFY(std::fabs(enu.n - 4.0) < 1e-6);
    QVERIFY(std::fabs(enu.u) < 1e-6);
    QVERIFY(std::fabs(gnss::horizontalDistance(enu) - 5.0) < 1e-6);

    // ENU -> ECEF 往返
    const gnss::Ecef back = gnss::enuToEcef(origin, enu);
    QVERIFY(std::fabs(back.x - point.x) < 1e-6);
    QVERIFY(std::fabs(back.y - point.y) < 1e-6);
    QVERIFY(std::fabs(back.z - point.z) < 1e-6);
}

void TestGnssCore::qualityNormalization()
{
    QCOMPARE(gnss::normalizeNmeaQuality(4), 1);  // RTK 固定
    QCOMPARE(gnss::normalizeNmeaQuality(5), 2);  // RTK 浮点
    QCOMPARE(gnss::normalizeNmeaQuality(2), 4);  // DGPS
    QCOMPARE(gnss::normalizeNmeaQuality(1), 5);  // 单点
    QCOMPARE(gnss::normalizeNmeaQuality(6), 6);  // 推算
    QCOMPARE(gnss::normalizeNmeaQuality(0), 0);  // 无效
    QCOMPARE(gnss::normalizeNmeaQuality(8), 0);  // 越界

    QVERIFY(gnss::isFix(1));
    QVERIFY(!gnss::isFix(5));      // 单点解不是固定解（旧绘图层把 5 当固定解）
    QVERIFY(gnss::isFloat(2));
    QCOMPARE(gnss::plotTypeOf(1), 0);
    QCOMPARE(gnss::plotTypeOf(2), 1);
    QCOMPARE(gnss::plotTypeOf(5), 2);
}

void TestGnssCore::ggaParserReadsPositionAndQuality()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString content =
        "$GNGGA,084246.98,3155.500000,N,11831.500000,E,4,38,1.4,58.3,M,0.0,M,23,*5A\n"
        "$GNGGA,084247.98,3155.500000,N,11831.500000,E,1,20,2.0,60.0,M,0.0,M,,*00\n";
    const QString path = writeTextFile(dir.path(), QStringLiteral("test.nmea"), content);
    QVERIFY(!path.isEmpty());

    const gnss::ParseResult parsed = gnss::parseGgaFile(path);
    QVERIFY2(parsed.ok, qPrintable(parsed.error));
    QCOMPARE(parsed.series.epochs.size(), 2);
    QVERIFY(!parsed.series.hasDate);  // GGA 本身无日期

    const gnss::GnssEpoch &first = parsed.series.epochs[0];
    QVERIFY(std::fabs(first.time - (8 * 3600 + 42 * 60 + 46.98)) < 1e-6);
    QVERIFY(std::fabs(first.lat - 31.925) < 1e-9);
    QVERIFY(std::fabs(first.lon - 118.525) < 1e-9);
    QVERIFY(std::fabs(first.height - 58.3) < 1e-9);
    QCOMPARE(first.quality, 1);   // GGA 4 -> Fix
    QCOMPARE(first.satellites, 38);
    QVERIFY(std::fabs(first.hdop - 1.4) < 1e-9);

    QCOMPARE(parsed.series.epochs[1].quality, 5);  // GGA 1 -> Single
}

void TestGnssCore::ggaParserRejectsInvalidQuality()
{
    QTemporaryDir dir;
    const QString content =
        "$GPGGA,000000.00,3155.500000,S,11831.500000,W,0,0,99.9,0.0,M,0.0,M,,*00\n";
    const QString path = writeTextFile(dir.path(), QStringLiteral("nofix.nmea"), content);

    const gnss::ParseResult parsed = gnss::parseGgaFile(path);
    // 只有一条无效定位：没有可用位置，视为解析失败
    QVERIFY(!parsed.ok);
    QVERIFY(parsed.series.epochs.isEmpty());
    QCOMPARE(parsed.skippedLines, 1);
}

void TestGnssCore::posParserKeepsDateAcrossMidnight()
{
    QTemporaryDir dir;
    const QString content =
        "% comment line\n"
        "% (Q = 1:fix ...)\n"
        "2025/07/02 23:59:59.00  -2613806.4358 4750189.5989 3348175.0003 1 30 0.0041 0.0058 0.0037 -0.0040 0.0038 0.0038 0.00\n"
        "2025/07/03 00:00:01.00  -2613806.4358 4750189.5989 3348175.0003 1 30 0.0041 0.0058 0.0037 -0.0040 0.0038 0.0038 0.00\n";
    const QString path = writeTextFile(dir.path(), QStringLiteral("late.pos"), content);

    const gnss::ParseResult parsed = gnss::parsePosFile(path);
    QVERIFY2(parsed.ok, qPrintable(parsed.error));
    QCOMPARE(parsed.series.epochs.size(), 2);
    QVERIFY(parsed.series.hasDate);

    // 关键回归：旧实现只保留"当天秒"，这两条会变成 86399 与 1，差值 -86398
    const double diff = parsed.series.epochs[1].time - parsed.series.epochs[0].time;
    QVERIFY2(std::fabs(diff - 2.0) < 1e-6, qPrintable(QString::number(diff)));
}

void TestGnssCore::posParserWithoutDateFallsBackToTimeOfDay()
{
    QTemporaryDir dir;
    const QString content =
        "23:59:59.00  -2613806.4358 4750189.5989 3348175.0003 1 30 0.0041 0.0058 0.0037 0 0 0 0\n";
    const QString path = writeTextFile(dir.path(), QStringLiteral("nodate.pos"), content);

    const gnss::ParseResult parsed = gnss::parsePosFile(path);
    QVERIFY(parsed.ok);
    QVERIFY(!parsed.series.hasDate);
    QVERIFY(!parsed.warnings.isEmpty());
}

void TestGnssCore::textReaderDetectsGbkEncoding()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // "评估" 的 GBK 字节 + 一条正常 GGA 语句
    QByteArray content = QByteArray::fromHex("c6c0b9c0");
    content += "\n$GNGGA,100000.00,3154.000000,N,11830.000000,E,4,20,1.0,50.0,M,0.0,M,,*00\n";

    const QString path = QDir(dir.path()).filePath(QStringLiteral("gbk.nmea"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();

    QCOMPARE(TextFileReader::detectEncoding(path), QStringLiteral("GB18030"));

    // 解码失败会整行乱码并导致零历元；能解析出来说明编码链路生效
    const gnss::ParseResult parsed = gnss::parseGgaFile(path);
    QVERIFY2(parsed.ok, qPrintable(parsed.error));
    QCOMPARE(parsed.series.epochs.size(), 1);
}

void TestGnssCore::normalizeUnwrapsBeforeSorting()
{
    gnss::EpochSeries series;
    series.hasDate = false;
    series.epochs << makeEpoch(86399.0, 1, 0, 0, false)
                  << makeEpoch(0.5, 1, 0, 0, false)
                  << makeEpoch(1.5, 1, 0, 0, false);

    const QStringList notes = gnss::normalizeSeries(series);
    QVERIFY(!notes.isEmpty());
    QCOMPARE(series.epochs.size(), 3);
    QVERIFY(std::fabs(series.epochs[0].time - 86399.0) < 1e-9);
    QVERIFY(std::fabs(series.epochs[1].time - 86400.5) < 1e-6);
    QVERIFY(std::fabs(series.epochs[2].time - 86401.5) < 1e-6);
}

void TestGnssCore::engineAlignsAcrossMidnight()
{
    const QVector<gnss::GnssEpoch> base = {
        makeEpoch(86399.0, 1, 0.0, 0.0, false),
        makeEpoch(0.5, 1, 0.0, 0.0, false),
        makeEpoch(1.5, 1, 0.0, 0.0, false),
    };
    const QVector<gnss::GnssEpoch> eval = {
        makeEpoch(86399.0, 1, 0.2, 0.0, false),
        makeEpoch(0.5, 1, 0.2, 0.0, false),
        makeEpoch(1.5, 1, 0.2, 0.0, false),
    };

    ev::EvaluationOptions options;
    options.thinInterval = 0.0;
    const ev::EvaluationResult result = ev::evaluate(makeSeries(base, false), makeSeries(eval, false), options);

    QCOMPARE(result.matchedCount, 3);
    QCOMPARE(result.matchedFixFix, 3);
    QVERIFY(std::fabs(result.fixFix.rmsPlane - 0.2) < 1e-6);
}

void TestGnssCore::engineAttachesDateToTimeOfDaySeries()
{
    double absolute = 0.0;
    QVERIFY(gnss::parseDateTime(QStringLiteral("2025/07/02"),
                                QStringLiteral("08:42:46.0"), &absolute));

    const QVector<gnss::GnssEpoch> base = {makeEpoch(absolute, 1, 0.0, 0.0, true)};
    const QVector<gnss::GnssEpoch> eval = {
        makeEpoch(8 * 3600 + 42 * 60 + 46.0, 1, 0.1, 0.0, false)};

    ev::EvaluationOptions options;
    options.thinInterval = 0.0;
    const ev::EvaluationResult result = ev::evaluate(makeSeries(base, true), makeSeries(eval, false), options);

    QCOMPARE(result.matchedCount, 1);
    QVERIFY(result.differentTimeBases);
    QVERIFY(result.alignedByTimeOfDay);
    QVERIFY(!result.alignmentNote.isEmpty());
}

void TestGnssCore::distributionDenominatorIsBaselineFix()
{
    QVector<gnss::GnssEpoch> base;
    QVector<gnss::GnssEpoch> eval;
    for (int i = 0; i < 20; ++i) {
        // 前 10 个基准为固定解，后 10 个为浮点解
        base << makeEpoch(1000.0 + i, i < 10 ? 1 : 2, 0.0, 0.0, true);
        eval << makeEpoch(1000.0 + i, 1, 0.1, 0.0, true);
    }

    ev::EvaluationOptions options;
    options.thinInterval = 0.0;
    const ev::EvaluationResult result = ev::evaluate(makeSeries(base, true), makeSeries(eval, true), options);

    QCOMPARE(result.matchedCount, 20);
    QCOMPARE(result.matchedBaselineFix, 10);
    QCOMPARE(result.fixFix.sampleCount, 10);
    QVERIFY(std::fabs(result.fixFix.rmsPlane - 0.1) < 1e-9);

    // 误差分布的分母必须是基准固定解样本（10），不是全部匹配点（20）。
    // 旧实现对 20 做分母，占比会被系统性压到 50%。
    QCOMPARE(result.planeTable.sampleCount, 10);
    QCOMPARE(result.planeTable.counts.last(), 10);         // < 1.00 m
    QVERIFY(std::fabs(result.planeTable.percent(result.planeTable.counts.size() - 1) - 100.0) < 1e-9);
}

void TestGnssCore::percentileDiffersFromStandardDeviation()
{
    QVector<double> values;
    QVector<double> heights;
    for (int i = 0; i < 10; ++i) {
        values << static_cast<double>(i);
        heights << 0.0;
    }

    const ev::ErrorStats stats = ev::computeStats(values, heights);
    QCOMPARE(stats.sampleCount, 10);
    QVERIFY(std::fabs(stats.rmsPlane - std::sqrt(28.5)) < 1e-9);   // sqrt(mean(i^2))
    QVERIFY(std::fabs(stats.stdPlane - std::sqrt(8.25)) < 1e-9);   // 总体标准差
    QVERIFY(std::fabs(stats.medianPlane - 4.5) < 1e-9);
    QVERIFY(std::fabs(stats.p68Plane - 6.1443) < 1e-3);            // 分位点 != σ
    QVERIFY(std::fabs(stats.cep95Plane - 8.55) < 1e-9);
    QVERIFY(std::fabs(stats.p68Plane - stats.stdPlane) > 1.0);     // 二者确实不同
}

void TestGnssCore::initializationTimeUsesFirstFix()
{
    QVector<gnss::GnssEpoch> eval;
    eval << makeEpoch(5000.0, 5, 0, 0, true);
    eval << makeEpoch(5001.0, 5, 0, 0, true);
    eval << makeEpoch(5030.0, 1, 0, 0, true);
    eval << makeEpoch(5031.0, 1, 0, 0, true);
    QVector<gnss::GnssEpoch> base = eval;
    for (gnss::GnssEpoch &epoch : base) {
        epoch.quality = 1;
    }

    ev::EvaluationOptions options;
    options.thinInterval = 0.0;
    const ev::EvaluationResult result = ev::evaluate(makeSeries(base, true), makeSeries(eval, true), options);

    QVERIFY(std::fabs(result.initializationTime - 30.0) < 1e-9);
    QCOMPARE(result.availabilityDenominator, 4);
}

void TestGnssCore::toleranceRejectsMismatchedEpochs()
{
    const QVector<gnss::GnssEpoch> base = {makeEpoch(1000.0, 1, 0, 0, true)};
    const QVector<gnss::GnssEpoch> eval = {makeEpoch(1000.5, 1, 0, 0, true)};

    ev::EvaluationOptions options;
    options.thinInterval = 0.0;
    options.matchTolerance = 0.1;
    ev::EvaluationResult result = ev::evaluate(makeSeries(base, true), makeSeries(eval, true), options);
    QCOMPARE(result.matchedCount, 0);
    QVERIFY(!result.valid);
    QVERIFY(!result.warnings.isEmpty());

    options.matchTolerance = 1.0;
    result = ev::evaluate(makeSeries(base, true), makeSeries(eval, true), options);
    QCOMPARE(result.matchedCount, 1);
}

void TestGnssCore::trajectoryCompletenessReflectsGap()
{
    gnss::EpochSeries series;
    series.hasDate = true;
    series.format = QStringLiteral("TEST");
    const double times[] = {0.0, 1.0, 2.0, 10.0, 11.0};
    for (double t : times) {
        series.epochs << makeEpoch(t, 1, 0, 0, true);
    }

    TrajectoryData data;
    QString error;
    QVERIFY2(data.buildFromSeries(series, &error), qPrintable(error));

    const TrackMeta meta = data.meta();
    QCOMPARE(meta.epochCount, 5);
    QVERIFY(std::fabs(meta.intervalSeconds - 1.0) < 1e-9);
    // 0..11 s、1 Hz 应有 12 个历元，实到 5 个 -> 41.7%
    QVERIFY2(std::fabs(meta.completenessPercent - 41.7) < 0.1,
             qPrintable(QString::number(meta.completenessPercent)));
    QVERIFY(meta.fixRatePercent > 99.9);
}

void TestGnssCore::projectRoundTrip()
{
    QTemporaryDir parent;
    QVERIFY(parent.isValid());

    ProjectManager project;
    QString error;
    QVERIFY2(project.create(QStringLiteral("case"), parent.path(), &error), qPrintable(error));

    const QString dataFile = writeTextFile(project.directory(), QStringLiteral("base.pos"),
                                           QStringLiteral("% dummy\n"));
    QVERIFY(!dataFile.isEmpty());

    ProjectManager::FileRecord record;
    record.role = ProjectManager::FileRole::Baseline;
    record.path = dataFile;
    record.displayName = QStringLiteral("base.pos (POS基准)");
    record.format = QStringLiteral("POS");
    record.epochCount = 1234;
    record.meta.format = record.format;
    record.meta.startTime = QStringLiteral("2025-07-02 00:00:00.000");
    record.meta.endTime = QStringLiteral("2025-07-02 01:00:00.000");
    record.meta.intervalSeconds = 1.0;
    record.meta.epochCount = record.epochCount;
    record.meta.fixRatePercent = 88.5;
    project.setFile(record);
    project.setResultFiles(QStringList() << QDir(project.directory()).filePath(QStringLiteral("evaluation_report.html")));
    QVERIFY2(project.save(&error), qPrintable(error));

    ProjectManager reopened;
    QVERIFY2(reopened.open(project.directory(), &error), qPrintable(error));
    QCOMPARE(reopened.name(), QStringLiteral("case"));
    QVERIFY(reopened.hasFile(ProjectManager::FileRole::Baseline));

    const ProjectManager::FileRecord loaded = reopened.file(ProjectManager::FileRole::Baseline);
    QCOMPARE(loaded.displayName, record.displayName);
    QCOMPARE(loaded.epochCount, 1234);
    QVERIFY(std::fabs(loaded.meta.fixRatePercent - 88.5) < 1e-9);
    QCOMPARE(loaded.meta.startTime, record.meta.startTime);

    // 结果文件不存在时不应被当成有效结果
    QVERIFY(reopened.resultFiles().isEmpty());

    // 旧配置（只有 Name/Path）也应能打开，只是没有文件记录
    QTemporaryDir legacy;
    QFile legacyConfig(QDir(legacy.path()).filePath(QStringLiteral("project.config")));
    QVERIFY(legacyConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    legacyConfig.write("[Project]\nName=old\nPath=somewhere\n");
    legacyConfig.close();

    ProjectManager legacyProject;
    QVERIFY2(legacyProject.open(legacy.path(), &error), qPrintable(error));
    QCOMPARE(legacyProject.name(), QStringLiteral("old"));
    QVERIFY(!legacyProject.hasFile(ProjectManager::FileRole::Baseline));
}

QTEST_GUILESS_MAIN(TestGnssCore)
#include "tst_bdsmep.moc"
