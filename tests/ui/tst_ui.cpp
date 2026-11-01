#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QStackedWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <random>

#include "camera2d.h"
#include "coordinates.h"
#include "filetableview.h"
#include "mainwindow.h"
#include "newprojectdialog.h"
#include "plotrenderer.h"
#include "plotstyle.h"
#include "projectmanager.h"
#include "projecttreewidget.h"
#include "renderframe.h"
#include "reportpreview.h"
#include "rtkplotwidget.h"
#include "rtkplottypes.h"
#include "trackindex.h"
#include "tracklod.h"
#include "trackrendermodel.h"
#include "trackseries.h"

namespace {

QString writeFile(const QString &directory, const QString &name, const QString &content)
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

// Liang-Barsky：线段是否与矩形相交（测试侧独立实现，不复用被测代码）
bool segmentIntersectsRect(const QPointF &a, const QPointF &b, const QRectF &rect)
{
    double t0 = 0.0;
    double t1 = 1.0;
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();

    const double p[4] = {-dx, dx, -dy, dy};
    const double q[4] = {a.x() - rect.left(), rect.right() - a.x(),
                         a.y() - rect.top(), rect.bottom() - a.y()};

    for (int i = 0; i < 4; ++i) {
        if (std::fabs(p[i]) < 1e-12) {
            if (q[i] < 0.0) {
                return false;
            }
            continue;
        }
        const double r = q[i] / p[i];
        if (p[i] < 0.0) {
            if (r > t1) return false;
            if (r > t0) t0 = r;
        } else {
            if (r < t0) return false;
            if (r < t1) t1 = r;
        }
    }
    return true;
}

// 5 个历元、1 Hz、与 GGA 时刻完全一致的 POS 基准
QString makePosContent()
{
    const gnss::Blh blh{31.9, 118.5, 50.0};
    const gnss::Ecef ecef = gnss::blhToEcef(blh);

    QString content = QStringLiteral("% test baseline\n");
    for (int i = 0; i < 5; ++i) {
        content += QStringLiteral("2025/07/02 10:00:0%1.00  %2  %3  %4  1 30  0.01 0.01 0.01 0 0 0 0\n")
                       .arg(i)
                       .arg(ecef.x, 0, 'f', 4)
                       .arg(ecef.y, 0, 'f', 4)
                       .arg(ecef.z, 0, 'f', 4);
    }
    return content;
}

// 与基准同位置的 GGA（quality=4 -> 固定解）
QString makeGgaContent()
{
    QString content;
    for (int i = 0; i < 5; ++i) {
        content += QStringLiteral(
                       "$GNGGA,10000%1.00,3154.000000,N,11830.000000,E,4,20,1.0,50.0,M,0.0,M,,*00\n")
                       .arg(i);
    }
    return content;
}

void saveProjectWithFiles(const QString &directory)
{
    ProjectManager project;
    QString error;
    QVERIFY2(project.create(QStringLiteral("ui-demo"), directory, &error), qPrintable(error));

    const QString posPath = writeFile(project.directory(), QStringLiteral("base.pos"), makePosContent());
    const QString ggaPath = writeFile(project.directory(), QStringLiteral("rover.nmea"), makeGgaContent());
    QVERIFY(!posPath.isEmpty());
    QVERIFY(!ggaPath.isEmpty());

    ProjectManager::FileRecord baseline;
    baseline.role = ProjectManager::FileRole::Baseline;
    baseline.path = posPath;
    baseline.displayName = QStringLiteral("base.pos (POS基准)");
    baseline.format = QStringLiteral("POS");
    baseline.epochCount = 5;
    baseline.meta.format = baseline.format;
    baseline.meta.epochCount = 5;
    baseline.meta.intervalSeconds = 1.0;
    baseline.meta.startTime = QStringLiteral("2025-07-02 10:00:00.000");
    baseline.meta.endTime = QStringLiteral("2025-07-02 10:00:04.000");
    baseline.meta.fixRatePercent = 100.0;
    baseline.meta.completenessPercent = 100.0;
    project.setFile(baseline);

    ProjectManager::FileRecord evaluation;
    evaluation.role = ProjectManager::FileRole::Evaluation;
    evaluation.path = ggaPath;
    evaluation.displayName = QStringLiteral("rover.nmea (GGA评估)");
    evaluation.format = QStringLiteral("GGA");
    evaluation.epochCount = 5;
    evaluation.meta.format = evaluation.format;
    evaluation.meta.epochCount = 5;
    evaluation.meta.intervalSeconds = 1.0;
    evaluation.meta.startTime = QStringLiteral("10:00:00.000");
    evaluation.meta.endTime = QStringLiteral("10:00:04.000");
    evaluation.meta.fixRatePercent = 100.0;
    evaluation.meta.completenessPercent = 100.0;
    project.setFile(evaluation);

    QVERIFY2(project.save(&error), qPrintable(error));
}

} // namespace

class TestUiSmoke : public QObject
{
    Q_OBJECT

private slots:
    void toolbarMatchesOriginalLayout();
    void newProjectButtonOpensDialog();
    void openProjectPopulatesTreeAndTable();
    void openInvalidProjectFailsGracefully();
    void evaluationProducesReport();
    void trackIsClippedAtCanvasEdge();
    void renderFrameCoversVisibleSegments();
    void visualSnapshot();
};

void TestUiSmoke::trackIsClippedAtCanvasEdge()
{
    // 构造一条穿过视口的轨迹，采样点与视口边界刻意错开：
    //   视口 world x = [5.0005, 5.0205]，点在 5.001 ... 5.020（口径内），
    //   5.000 与 5.021 刚好在视口外。旧实现只画 5.001..5.020，
    //   屏幕上是第 10..390 列，左右各留 10 px 空白；新实现应画到第 0/399 列。
    QVector<TrajectoryPoint> points;
    points.reserve(20001);
    for (int i = 0; i <= 20000; ++i) {
        const double x = -10.0 + i * 0.001;
        TrajectoryPoint point;
        point.x = x;
        point.y = 0.5 * std::sin(x);
        point.time = 1000.0 + i;
        // 质量 0 的标记色是白色，等于"只画线"，避免标记干扰边缘像素判断
        point.quality = 0;
        point.type = 0;
        points.append(point);
    }

    TrackSeries series;
    series.setPoints(points);

    TrackIndex index;
    index.rebuild(series);

    const double centerY = 0.5 * std::sin(5.0105);

    Camera2D camera;
    camera.setViewport(QSize(400, 200));
    camera.setCenter(QPointF(5.0105, centerY));
    camera.setScale(20000.0);          // 400 px 宽 -> 0.02 m 视口

    LodPolicy lod;
    lod.maxPoints = 10000;

    RenderOptions options;
    options.showMarkers = false;       // 只验证轨迹线
    options.highQuality = true;

    const RenderFrame frame = TrackRenderModel::build(series, index, camera, lod, options);
    QVERIFY2(!frame.polylines.isEmpty(), "视口内没有生成轨迹折线");

    // 契约：折线必须越过画布左右边界（由渲染层裁齐），而不是停在视口内
    double minX = 1e18;
    double maxX = -1e18;
    for (const QPolygonF &polyline : frame.polylines) {
        for (const QPointF &point : polyline) {
            minX = std::min(minX, point.x());
            maxX = std::max(maxX, point.x());
        }
    }
    QVERIFY2(minX <= 0.0, qPrintable(QStringLiteral("左端只到 %1 px").arg(minX)));
    QVERIFY2(maxX >= 399.0, qPrintable(QStringLiteral("右端只到 %1 px").arg(maxX)));

    PlotStyle style;
    style.showTrack = true;
    style.showMarkers = false;
    style.showGrid = false;
    style.showScale = false;
    style.trackColor = Qt::black;
    style.background = Qt::white;
    style.lineWidth = 2.0;
    style.renderLevel = 1;

    QImage image(400, 200, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    PlotRenderer::paintFrame(painter, frame, style);
    painter.end();

    // 画布左右两列必须有线（被裁剪后的半截轨迹），而不是空白
    const auto columnHasInk = [&image](int x) {
        for (int y = 0; y < image.height(); ++y) {
            if (image.pixelColor(x, y) != QColor(Qt::white)) {
                return true;
            }
        }
        return false;
    };

    QVERIFY2(columnHasInk(0), "轨迹没有画到左边界（被提前裁掉了）");
    QVERIFY2(columnHasInk(image.width() - 1), "轨迹没有画到右边界（被提前裁掉了）");
}

void TestUiSmoke::renderFrameCoversVisibleSegments()
{
    // 契约测试：任何与视口相交的线段，都必须出现在 RenderFrame 里。
    // 随机轨迹 + 20 组随机视口，独立用 Liang-Barsky 计算"应该出现"的线段，
    // 再逐条检查两个端点是否都在帧的折线顶点里。
    std::mt19937 rng(20261018);
    std::uniform_real_distribution<double> step(-2.0, 2.0);

    QVector<TrajectoryPoint> points;
    points.reserve(400);
    double x = 0.0;
    double y = 0.0;
    for (int i = 0; i < 400; ++i) {
        x += step(rng);
        y += step(rng);
        TrajectoryPoint point;
        point.x = x;
        point.y = y;
        point.time = 1000.0 + i;
        point.quality = 1;
        point.type = 0;
        points.append(point);
    }

    TrackSeries series;
    series.setPoints(points);

    TrackIndex index;
    index.rebuild(series);

    const QRectF bounds = series.bounds();
    std::uniform_real_distribution<double> centerX(bounds.left(), bounds.right());
    std::uniform_real_distribution<double> centerY(bounds.top(), bounds.bottom());
    std::uniform_real_distribution<double> scale(2.0, 20.0);

    Camera2D camera;
    camera.setViewport(QSize(320, 240));

    LodPolicy lod;
    lod.maxPoints = series.size() + 10;   // 不做抽稀，专门验证可见性

    RenderOptions options;
    options.showMarkers = false;
    options.highQuality = true;

    for (int trial = 0; trial < 20; ++trial) {
        camera.setCenter(QPointF(centerX(rng), centerY(rng)));
        camera.setScale(scale(rng));

        const RenderFrame frame = TrackRenderModel::build(series, index, camera, lod, options);

        // 先独立算出这个视口里"应该出现"的线段
        QVector<int> expectedSegments;
        const QRectF visible = camera.visibleWorld();
        for (int i = 1; i < points.size(); ++i) {
            const QPointF a(points[i - 1].x, points[i - 1].y);
            const QPointF b(points[i].x, points[i].y);
            if (segmentIntersectsRect(a, b, visible)) {
                expectedSegments.append(i);
            }
        }

        if (expectedSegments.isEmpty()) {
            // 视口完全落在轨迹之外：空帧是正确结果
            continue;
        }

        QVERIFY2(!frame.polylines.isEmpty(), "视口内有相交线段，但没有生成折线");

        QVector<QPointF> vertices;
        for (const QPolygonF &polyline : frame.polylines) {
            for (const QPointF &point : polyline) {
                vertices.append(point);
            }
        }

        const auto contains = [&vertices](const QPointF &point) {
            for (const QPointF &vertex : vertices) {
                if (QLineF(vertex, point).length() < 1e-6) {
                    return true;
                }
            }
            return false;
        };

        for (int i : expectedSegments) {
            const QPointF a(points[i - 1].x, points[i - 1].y);
            const QPointF b(points[i].x, points[i].y);
            const QPointF screenA = camera.worldToScreen(a);
            const QPointF screenB = camera.worldToScreen(b);
            QVERIFY2(contains(screenA),
                     qPrintable(QStringLiteral("trial %1: 线段 %2 的起点缺失").arg(trial).arg(i)));
            QVERIFY2(contains(screenB),
                     qPrintable(QStringLiteral("trial %1: 线段 %2 的终点缺失").arg(trial).arg(i)));
        }
    }

    // 极端情况：视口内一个点都没有，但一条长线段横穿画布。
    // 只有"线段-矩形求交"的兜底路径能把这条线补出来。
    QVector<TrajectoryPoint> spike;
    for (int i = 0; i < 2; ++i) {
        TrajectoryPoint point;
        point.x = (i == 0) ? -1000.0 : 1000.0;
        point.y = 0.0;
        point.time = 1000.0 + i;
        point.quality = 1;
        point.type = 0;
        spike.append(point);
    }

    TrackSeries spikeSeries;
    spikeSeries.setPoints(spike);
    TrackIndex spikeIndex;
    spikeIndex.rebuild(spikeSeries);

    Camera2D spikeCamera;
    spikeCamera.setViewport(QSize(200, 100));
    spikeCamera.setCenter(QPointF(0.0, 0.0));
    spikeCamera.setScale(1.0);   // 视口 [-100,100] x [-50,50]

    const RenderFrame spikeFrame =
        TrackRenderModel::build(spikeSeries, spikeIndex, spikeCamera, lod, options);
    QVERIFY2(!spikeFrame.polylines.isEmpty(), "横穿视口的长线段没有被补出来");
}

void TestUiSmoke::visualSnapshot()
{
    // 手动视觉回归：设置 BDSMEP_SNAPSHOT_DIR 后运行，会输出主界面与报表页截图
    const QString outputDir = qEnvironmentVariable("BDSMEP_SNAPSHOT_DIR");
    if (outputDir.isEmpty()) {
        QSKIP("未设置 BDSMEP_SNAPSHOT_DIR，跳过截图");
    }
    QDir().mkpath(outputDir);

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    saveProjectWithFiles(temp.path());

    MainWindow window;
    QVERIFY(window.openProjectPath(QDir(temp.path()).filePath(QStringLiteral("ui-demo"))));
    window.show();
    QTest::qWait(150);

    QVERIFY(window.grab().save(QDir(outputDir).filePath(QStringLiteral("main.png"))));

    auto *stack = window.findChild<QStackedWidget *>();
    QVERIFY(stack);
    stack->setCurrentIndex(1);
    QTest::qWait(150);
    QVERIFY(window.grab().save(QDir(outputDir).filePath(QStringLiteral("report.png"))));

    // 轨迹图：给绘图控件喂一段合成轨迹，确认网格/标记/配色正常
    auto *plot = window.findChild<RTKPlotWidget *>();
    QVERIFY(plot);
    QVector<TrajectoryPoint> track;
    for (int i = 0; i < 600; ++i) {
        TrajectoryPoint point;
        const double angle = i * 0.03;
        point.x = (40.0 + 12.0 * std::cos(angle)) * std::cos(angle * 0.2);
        point.y = (40.0 + 12.0 * std::cos(angle)) * std::sin(angle * 0.2);
        point.time = 1000.0 + i;
        point.quality = (i % 89 == 0) ? 2 : 1;
        point.type = point.quality == 1 ? 0 : 1;
        track.append(point);
    }
    plot->setTrajectoryData(track);
    stack->setCurrentIndex(3);
    QTest::qWait(200);
    QVERIFY(window.grab().save(QDir(outputDir).filePath(QStringLiteral("plot.png"))));
}

void TestUiSmoke::toolbarMatchesOriginalLayout()
{
    MainWindow window;

    auto *toolbar = window.findChild<QToolBar *>();
    QVERIFY(toolbar);

    QStringList labels;
    int separators = 0;
    for (QAction *action : toolbar->actions()) {
        if (action->isSeparator()) {
            ++separators;
            continue;
        }
        QVERIFY2(action->isEnabled(), qPrintable(action->text()));
        labels << action->text();
    }

    // 与原版一致的 5 个按钮 + 2 个分隔符，且全部保持可用
    QCOMPARE(separators, 2);
    QCOMPARE(labels, QStringList() << QStringLiteral("新建")
                                   << QStringLiteral("导入基准")
                                   << QStringLiteral("导入评估")
                                   << QStringLiteral("数据评估")
                                   << QStringLiteral("报表结果"));
    QCOMPARE(toolbar->iconSize(), QSize(32, 32));
}

void TestUiSmoke::newProjectButtonOpensDialog()
{
    MainWindow window;

    auto *toolbar = window.findChild<QToolBar *>();
    QVERIFY(toolbar);

    QToolButton *newButton = nullptr;
    for (QToolButton *button : toolbar->findChildren<QToolButton *>()) {
        if (button->text().contains(QStringLiteral("新建"))) {
            newButton = button;
            break;
        }
    }
    QVERIFY2(newButton, "工具栏里找不到“新建”按钮");

    bool sawDialog = false;
    QTimer closer;
    closer.setInterval(20);
    QObject::connect(&closer, &QTimer::timeout, [&sawDialog, &closer]() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *dialog = qobject_cast<NewProjectDialog *>(widget)) {
                sawDialog = true;
                closer.stop();
                dialog->reject();
            }
        }
    });
    closer.start();

    QTest::mouseClick(newButton, Qt::LeftButton);
    closer.stop();

    // 点击必须真的弹出新建项目对话框（回归"按钮点了没反应"）
    QVERIFY2(sawDialog, "点击“新建”没有弹出新建项目对话框");
}

void TestUiSmoke::openProjectPopulatesTreeAndTable()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    saveProjectWithFiles(temp.path());

    MainWindow window;
    QVERIFY(window.openProjectPath(QDir(temp.path()).filePath(QStringLiteral("ui-demo"))));
    QVERIFY(window.windowTitle().contains(QStringLiteral("ui-demo")));

    auto *tree = window.findChild<ProjectTreeWidget *>();
    QVERIFY(tree);
    QVERIFY(tree->hasDataFile(ProjectTreeWidget::ItemKind::Baseline));
    QVERIFY(tree->hasDataFile(ProjectTreeWidget::ItemKind::Evaluation));

    auto *table = window.findChild<FileTableView *>();
    QVERIFY(table);
    auto *view = table->findChild<QTableView *>();
    QVERIFY(view);
    QVERIFY(view->model());
    QCOMPARE(view->model()->rowCount(), 2);

    // 打开不存在的目录：返回 false，不弹模态框
    QVERIFY(!window.openProjectPath(QDir(temp.path()).filePath(QStringLiteral("missing"))));
}

void TestUiSmoke::openInvalidProjectFailsGracefully()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    MainWindow window;
    QVERIFY(!window.openProjectPath(temp.path()));
    QVERIFY(!window.windowTitle().contains(QStringLiteral("—")));
}

void TestUiSmoke::evaluationProducesReport()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    saveProjectWithFiles(temp.path());

    MainWindow window;
    QVERIFY(window.openProjectPath(QDir(temp.path()).filePath(QStringLiteral("ui-demo"))));

    // 评估结束会弹信息框；自动化里定时关闭，避免阻塞
    QTimer closer;
    QObject::connect(&closer, &QTimer::timeout, []() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                box->accept();
            }
        }
    });
    closer.start(30);

    // 点工具栏按钮，而不是直接调槽：验证的是"按钮 → 槽"这条链路
    auto *toolbar = window.findChild<QToolBar *>();
    QVERIFY(toolbar);
    QToolButton *evaluateButton = nullptr;
    for (QToolButton *button : toolbar->findChildren<QToolButton *>()) {
        if (button->text().contains(QStringLiteral("数据评估"))) {
            evaluateButton = button;
            break;
        }
    }
    QVERIFY2(evaluateButton, "工具栏里找不到“数据评估”按钮");
    QVERIFY(evaluateButton->isEnabled());
    QTest::mouseClick(evaluateButton, Qt::LeftButton);

    const QString projectDir = QDir(temp.path()).filePath(QStringLiteral("ui-demo"));
    const QString reportPath = QDir(projectDir).filePath(QStringLiteral("evaluation_report.html"));

    QElapsedTimer timer;
    timer.start();
    while (!QFile::exists(reportPath) && timer.elapsed() < 20000) {
        QTest::qWait(50);
    }
    closer.stop();

    QVERIFY2(QFile::exists(reportPath), "评估未在 20 s 内生成报告");

    const QString textPath = QDir(projectDir).filePath(QStringLiteral("evaluation_report.txt"));
    QFile text(textPath);
    QVERIFY(text.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&text);
#if !defined(QT6)
    in.setCodec("UTF-8");
#endif
    const QString report = in.readAll();
    text.close();

    QVERIFY2(report.contains(QStringLiteral("GNSS 数据评估报告")), qPrintable(report.left(200)));
    QVERIFY(report.contains(QStringLiteral("基准固定解匹配      : 5")));
    QVERIFY(report.contains(QStringLiteral("固定解-固定解")));

    // 明细文件
    QVERIFY(QFile::exists(QDir(projectDir).filePath(QStringLiteral("Fix_Fix.txt"))));

    // 结果已登记到任务树与项目配置
    auto *tree = window.findChild<ProjectTreeWidget *>();
    QVERIFY(tree);
    QVERIFY2(tree->resultCount() >= 5, qPrintable(QString::number(tree->resultCount())));

    ProjectManager reopened;
    QString error;
    QVERIFY2(reopened.open(projectDir, &error), qPrintable(error));
    QVERIFY(reopened.resultFiles().size() >= 5);

    auto *preview = window.findChild<ReportPreview *>();
    QVERIFY(preview);
    QVERIFY(preview->html().contains(QStringLiteral("GNSS")));

    // “报表结果”按钮应当切回报表视图（不是弹框）
    QToolButton *reportButton = nullptr;
    for (QToolButton *button : toolbar->findChildren<QToolButton *>()) {
        if (button->text().contains(QStringLiteral("报表结果"))) {
            reportButton = button;
            break;
        }
    }
    QVERIFY(reportButton);
    auto *stack = window.findChild<QStackedWidget *>();
    QVERIFY(stack);
    stack->setCurrentIndex(0);
    QTest::mouseClick(reportButton, Qt::LeftButton);
    QCOMPARE(stack->currentIndex(), 1);
}

int main(int argc, char *argv[])
{
    // 默认无显示环境（CI / 远程会话）；想截真实字体图可显式指定 QT_QPA_PLATFORM=windows
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("BDS_MEP_TEST"));
    QCoreApplication::setApplicationName(QStringLiteral("BDS_MEP_UI_TEST"));

    TestUiSmoke test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_ui.moc"
