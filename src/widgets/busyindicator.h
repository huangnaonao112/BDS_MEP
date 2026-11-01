#ifndef BUSYINDICATOR_H
#define BUSYINDICATOR_H

#include <QWidget>

class QProgressBar;
class QPushButton;

// 状态栏里的"忙碌指示"：进度条 + 取消按钮。
// 空闲时完全隐藏，不改变原界面外观。
class BusyIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit BusyIndicator(QWidget *parent = nullptr);

    // progress < 0 表示不确定进度（解析等没有进度回调的阶段）
    void apply(bool busy, bool cancellable, int progress);

signals:
    void cancelRequested();

private:
    QProgressBar *m_progress = nullptr;
    QPushButton *m_cancel = nullptr;
};

#endif // BUSYINDICATOR_H
