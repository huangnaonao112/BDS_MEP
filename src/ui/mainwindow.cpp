#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "appcontroller.h"
#include "busyindicator.h"
#include "debugoutput.h"
#include "filetableview.h"
#include "gnssdefs.h"
#include "newprojectdialog.h"
#include "reportpreview.h"
#include "rtkplotwidget.h"
#include "version.h"

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>

#include <cmath>

namespace {

QVector<TrajectoryPoint> toPlotPoints(const TrajectoryData &data)
{
    QVector<TrajectoryPoint> points;
    const auto &raw = data.getPoints();
    points.reserve(raw.size());

    double lastTime = -1e18;
    for (const CoordinatePoint &point : raw) {
        // 绘图抽稀到 1 Hz：渲染层再做 LOD，两层各司其职
        if (point.time - lastTime < 0.999) {
            continue;
        }
        lastTime = point.time;

        TrajectoryPoint plotPoint;
        plotPoint.x = point.x;
        plotPoint.y = point.y;
        plotPoint.z = point.z;
        plotPoint.time = point.time;
        plotPoint.quality = point.quality;
        plotPoint.type = gnss::plotTypeOf(point.quality);
        plotPoint.sdx = point.sdx;
        plotPoint.sdy = point.sdy;
        plotPoint.sdz = point.sdz;
        plotPoint.sdxy = point.sdxy;
        plotPoint.sdyz = point.sdyz;
        plotPoint.sdzx = point.sdzx;
        plotPoint.hdop = point.hdop;
        points.append(plotPoint);
    }
    return points;
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream in(&file);
#if !defined(QT6)
    in.setCodec("UTF-8");
#endif
    return in.readAll();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_controller = new AppController(this);

    setupActions();
    setupMainLayout();
    setupViews();
    setupStatusBar();
    connectSignals();

    setWindowIcon(QIcon(QStringLiteral(":/icons/icons/weizhi.png")));
    resize(1200, 800);

    renderState(m_controller->state());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupActions()
{
    // 与原版完全一致的工具栏：5 个按钮、32px 图标、图标下文字。
    // 打开项目不占工具栏位置，走 Ctrl+O 与任务树右键（不改变外观）。
    m_newAction = new QAction(QIcon(QStringLiteral(":/icons/icons/xinjian.png")),
                              QStringLiteral("新建"), this);
    m_importBaselineAction = new QAction(QIcon(QStringLiteral(":/icons/icons/daoru.png")),
                                         QStringLiteral("导入基准"), this);
    m_importEvaluationAction = new QAction(QIcon(QStringLiteral(":/icons/icons/daoru.png")),
                                           QStringLiteral("导入评估"), this);
    m_evaluateAction = new QAction(QIcon(QStringLiteral(":/icons/icons/shujupinggu.png")),
                                   QStringLiteral("数据评估"), this);
    m_reportAction = new QAction(QIcon(QStringLiteral(":/icons/icons/baobiaoxiazai.png")),
                                 QStringLiteral("报表结果"), this);

    m_openAction = new QAction(QStringLiteral("打开项目"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    addAction(m_openAction);   // 挂在窗口上以获得快捷键，但不显示按钮

    QToolBar *toolBar = addToolBar(QStringLiteral("主工具栏"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(32, 32));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    toolBar->addAction(m_newAction);
    toolBar->addSeparator();
    toolBar->addAction(m_importBaselineAction);
    toolBar->addAction(m_importEvaluationAction);
    toolBar->addSeparator();
    toolBar->addAction(m_evaluateAction);
    toolBar->addAction(m_reportAction);
}

void MainWindow::setupMainLayout()
{
    // 与原版一致的分栏策略：下方输出区占 3 份，任务树与视图区各占一半
    ui->mainSplitter->setStretchFactor(0, 1);
    ui->mainSplitter->setStretchFactor(1, 3);
    ui->topSplitter->setStretchFactor(0, 1);
    ui->topSplitter->setStretchFactor(1, 1);

    QTimer::singleShot(100, this, [this]() {
        QList<int> mainSizes;
        mainSizes << 600 << 200;
        ui->mainSplitter->setSizes(mainSizes);
    });

    QList<int> topSizes = ui->topSplitter->sizes();
    if (topSizes.size() == 2) {
        const int totalWidth = ui->topSplitter->width();
        topSizes[0] = totalWidth / 2;
        topSizes[1] = totalWidth / 2;
        ui->topSplitter->setSizes(topSizes);
    }
}

void MainWindow::setupViews()
{
    m_plot = new RTKPlotWidget(this);
    ui->viewStackedWidget->addWidget(m_plot);
    ui->viewStackedWidget->setCurrentIndex(FilesPage);
}

void MainWindow::setupStatusBar()
{
    m_busy = new BusyIndicator(this);
    statusBar()->addPermanentWidget(m_busy);
    statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::connectSignals()
{
    connect(m_newAction, &QAction::triggered, this, &MainWindow::onNewProject);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenProject);
    connect(m_importBaselineAction, &QAction::triggered, this, &MainWindow::onImportBaseline);
    connect(m_importEvaluationAction, &QAction::triggered, this, &MainWindow::onImportEvaluation);
    connect(m_evaluateAction, &QAction::triggered, this, &MainWindow::onEvaluate);
    connect(m_reportAction, &QAction::triggered, this, &MainWindow::onShowReport);

    connect(ui->projectTreeWidget, &ProjectTreeWidget::itemSelected,
            this, &MainWindow::onTreeItemSelected);
    connect(ui->projectTreeWidget, &ProjectTreeWidget::deleteRequested,
            this, &MainWindow::onTreeDeleteRequested);
    connect(ui->projectTreeWidget, &ProjectTreeWidget::openProjectRequested,
            this, &MainWindow::onOpenProject);

    connect(ui->viewStackedWidget, &QStackedWidget::currentChanged, this, [this](int index) {
        static const char *names[] = {"文件详情视图", "报表预览视图", "评估结果报表", "轨迹图视图"};
        if (index >= 0 && index < 4) {
            log(QStringLiteral("切换到%1").arg(QString::fromUtf8(names[index])));
        }
    });

    connect(m_controller, &AppController::stateChanged, this, &MainWindow::renderState);
    connect(m_controller, &AppController::logMessage, this, &MainWindow::log);
    connect(m_controller, &AppController::alertRequested, this, &MainWindow::showAlert);
    connect(m_controller, &AppController::statusMessage, this, [this](const QString &text) {
        statusBar()->showMessage(text, 8000);
    });
    connect(m_controller, &AppController::progressChanged, this, [this](int percent) {
        m_progress = percent;
        if (m_state.busy) {
            m_busy->apply(true, m_state.cancellable, percent);
        }
    });
    connect(m_controller, &AppController::trajectoryReady, this, &MainWindow::onTrajectoryReady);
    connect(m_controller, &AppController::reportReady, this, &MainWindow::onReportReady);
    connect(m_busy, &BusyIndicator::cancelRequested, this, &MainWindow::onCancelEvaluation);
}

void MainWindow::renderState(const AppState &state)
{
    m_state = state;

    QString title = QStringLiteral("%1 v%2")
                        .arg(QStringLiteral(APP_DISPLAY_NAME), QStringLiteral(APP_VERSION));
    if (state.hasProject) {
        title += QStringLiteral(" — %1").arg(state.projectName);
    }
    setWindowTitle(title);

    ui->projectTreeWidget->rebuild();
    if (state.hasProject) {
        ui->projectTreeWidget->setProjectName(state.projectName);
        for (const ProjectManager::FileRecord &record : state.files) {
            const auto kind = record.role == ProjectManager::FileRole::Baseline
                                  ? ProjectTreeWidget::ItemKind::Baseline
                                  : ProjectTreeWidget::ItemKind::Evaluation;
            ui->projectTreeWidget->addDataFile(kind, record.displayName, record.path);
        }
        for (const QString &path : state.resultFiles) {
            ui->projectTreeWidget->addResultFile(QFileInfo(path).fileName(), path);
        }
    }

    ui->fileTableView->setFiles(state.files);

    if (state.busy) {
        m_busy->apply(true, state.cancellable, m_progress);
    } else {
        m_progress = -1;
        m_busy->apply(false, false, 0);
    }

    if (!state.hasProject) {
        m_plot->clearData();
        m_currentTrajectoryPath.clear();
        clearReportViews();
    }
}

void MainWindow::showAlert(const QString &title, const QString &text, const QString &level)
{
    if (level == QLatin1String("error") || level == QLatin1String("critical")) {
        QMessageBox::critical(this, title, text);
    } else if (level == QLatin1String("warning")) {
        QMessageBox::warning(this, title, text);
    } else {
        QMessageBox::information(this, title, text);
    }
}

void MainWindow::onTrajectoryReady(const QString &path, const TrajectoryData &data, bool reveal)
{
    m_plot->setTrajectoryData(toPlotPoints(data));
    m_currentTrajectoryPath = path;

    if (reveal) {
        ui->viewStackedWidget->setCurrentIndex(PlotPage);
        statusBar()->showMessage(QStringLiteral("轨迹：%1").arg(QFileInfo(path).fileName()), 8000);
    }
}

void MainWindow::onReportReady()
{
    ui->reportPreview->setHtml(m_controller->reportHtml());
    ui->reportTextEdit->setPlainText(m_controller->reportText());
    ui->viewStackedWidget->setCurrentIndex(ReportPage);
    statusBar()->showMessage(QStringLiteral("评估完成，报告已生成"), 8000);
}

void MainWindow::onNewProject()
{
    if (m_state.hasProject) {
        QMessageBox::warning(this, QStringLiteral("新建项目"),
                             QStringLiteral("已存在任务，无法创建多个任务！\n"
                                            "请先在任务树右键“关闭并清空项目”。"));
        log(QStringLiteral("新建项目失败: 已存在任务"), QStringLiteral("error"));
        return;
    }

    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_controller->createProject(dialog.getProjectName(), dialog.getProjectPath());
}

void MainWindow::onOpenProject()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("打开项目（选择包含 project.config 的目录）"), QString());
    if (directory.isEmpty()) {
        return;
    }
    if (!openProjectPath(directory)) {
        QMessageBox::warning(this, QStringLiteral("打开项目"),
                             QStringLiteral("无法打开项目：%1").arg(m_controller->lastError()));
    }
}

bool MainWindow::openProjectPath(const QString &directory)
{
    return m_controller->openProject(directory);
}

bool MainWindow::hasFile(ProjectManager::FileRole role) const
{
    for (const ProjectManager::FileRecord &record : m_state.files) {
        if (record.role == role) {
            return true;
        }
    }
    return false;
}

void MainWindow::onImportBaseline()
{
    const auto role = ProjectManager::FileRole::Baseline;
    if (!m_state.hasProject) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("请先新建项目再导入基准文件！"));
        return;
    }
    if (hasFile(role)) {
        QMessageBox::warning(this, QStringLiteral("导入限制"),
                             QStringLiteral("每个项目只能导入一个基准文件！\n"
                                            "请先删除现有基准文件后再导入新文件。"));
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入基准文件"), QString(),
        QStringLiteral("解决方案文件 (*.pos *.stat *.nmea *.txt *.ubx);;所有文件 (*.*)"));
    if (!fileName.isEmpty()) {
        m_controller->importFile(role, fileName);
    }
}

void MainWindow::onImportEvaluation()
{
    const auto role = ProjectManager::FileRole::Evaluation;
    if (!m_state.hasProject) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("请先新建项目再导入评估文件！"));
        return;
    }
    if (hasFile(role)) {
        QMessageBox::warning(this, QStringLiteral("导入限制"),
                             QStringLiteral("每个项目只能导入一个评估文件！\n"
                                            "请先删除现有评估文件后再导入新文件。"));
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入评估文件"), QString(),
        QStringLiteral("解决方案文件 (*.pos *.stat *.nmea *.txt *.ubx);;所有文件 (*.*)"));
    if (!fileName.isEmpty()) {
        m_controller->importFile(role, fileName);
    }
}

void MainWindow::onEvaluate()
{
    m_controller->evaluate();
}

void MainWindow::onShowReport()
{
    const QString html = m_controller->reportHtml();
    if (!html.isEmpty()) {
        ui->reportPreview->setHtml(html);
    } else {
        ui->reportPreview->clear();
    }
    ui->viewStackedWidget->setCurrentIndex(ReportPage);
}

void MainWindow::onCancelEvaluation()
{
    m_controller->cancelEvaluation();
}

void MainWindow::onTreeItemSelected(ProjectTreeWidget::ItemKind kind, const QString &path,
                                    const QString &name)
{
    Q_UNUSED(name)
    switch (kind) {
    case ProjectTreeWidget::ItemKind::Baseline:
    case ProjectTreeWidget::ItemKind::Evaluation:
        m_controller->loadTrajectory(path);
        break;
    case ProjectTreeWidget::ItemKind::Result:
        showResultFile(path);
        break;
    default:
        ui->viewStackedWidget->setCurrentIndex(FilesPage);
        break;
    }
}

void MainWindow::onTreeDeleteRequested(ProjectTreeWidget::ItemKind kind, const QString &path,
                                       const QString &name)
{
    if (kind == ProjectTreeWidget::ItemKind::Project) {
        m_controller->closeProject();
        log(QStringLiteral("项目已清空：%1").arg(name), QStringLiteral("warning"));
        return;
    }

    if (kind == ProjectTreeWidget::ItemKind::Result) {
        m_controller->removeResult(path);
        return;
    }

    const auto role = kind == ProjectTreeWidget::ItemKind::Baseline
                          ? ProjectManager::FileRole::Baseline
                          : ProjectManager::FileRole::Evaluation;
    if (path == m_currentTrajectoryPath) {
        m_plot->clearData();
        m_currentTrajectoryPath.clear();
    }
    clearReportViews();
    m_controller->removeFile(role);
}

void MainWindow::showResultFile(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    const QString content = readTextFile(path);
    if (content.isEmpty() && !QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("打开结果"),
                             QStringLiteral("文件不存在：%1").arg(path));
        return;
    }

    if (QFileInfo(path).suffix().compare(QStringLiteral("html"), Qt::CaseInsensitive) == 0) {
        ui->reportPreview->setHtml(content);
        ui->viewStackedWidget->setCurrentIndex(ReportPage);
    } else {
        ui->reportTextEdit->setPlainText(content);
        ui->viewStackedWidget->setCurrentIndex(TextPage);
    }
    statusBar()->showMessage(QStringLiteral("查看：%1").arg(QFileInfo(path).fileName()), 8000);
}

void MainWindow::clearReportViews()
{
    ui->reportPreview->clear();
    ui->reportTextEdit->clear();
}

void MainWindow::log(const QString &message, const QString &level)
{
    if (ui && ui->debugOutputWidget) {
        ui->debugOutputWidget->appendMessage(message, level);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_controller->cancelEvaluation();
    QMainWindow::closeEvent(event);
}
