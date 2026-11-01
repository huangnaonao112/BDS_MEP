#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include "appstate.h"
#include "projecttreewidget.h"
#include "trajectorydata.h"

class AppController;
class BusyIndicator;
class RTKPlotWidget;
class QAction;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 被动视图：只把 AppState 画出来、把用户操作转成 AppController 的命令。
// 业务校验/缓存/文案在控制器里，这里不再直接依赖任何 Service。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openProjectPath(const QString &directory);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewProject();
    void onOpenProject();
    void onImportBaseline();
    void onImportEvaluation();
    void onEvaluate();
    void onShowReport();
    void onCancelEvaluation();

    void onTreeItemSelected(ProjectTreeWidget::ItemKind kind, const QString &path, const QString &name);
    void onTreeDeleteRequested(ProjectTreeWidget::ItemKind kind, const QString &path, const QString &name);

    void renderState(const AppState &state);
    void showAlert(const QString &title, const QString &text, const QString &level);
    void onTrajectoryReady(const QString &path, const TrajectoryData &data, bool reveal);
    void onReportReady();

private:
    enum Page {
        FilesPage = 0,
        ReportPage = 1,
        TextPage = 2,
        PlotPage = 3
    };

    void setupActions();
    void setupMainLayout();
    void setupStatusBar();
    void setupViews();
    void connectSignals();

    void log(const QString &message, const QString &level = QStringLiteral("info"));
    void showResultFile(const QString &path);
    void clearReportViews();
    bool hasFile(ProjectManager::FileRole role) const;

    Ui::MainWindow *ui = nullptr;
    AppController *m_controller = nullptr;
    RTKPlotWidget *m_plot = nullptr;
    BusyIndicator *m_busy = nullptr;

    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_importBaselineAction = nullptr;
    QAction *m_importEvaluationAction = nullptr;
    QAction *m_evaluateAction = nullptr;
    QAction *m_reportAction = nullptr;

    AppState m_state;
    int m_progress = -1;
    QString m_currentTrajectoryPath;
};

#endif // MAINWINDOW_H
