#include "busyindicator.h"

#include <QHBoxLayout>
#include <QProgressBar>
#include <QPushButton>

BusyIndicator::BusyIndicator(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_progress = new QProgressBar(this);
    m_progress->setFixedWidth(180);
    m_progress->setTextVisible(true);
    layout->addWidget(m_progress);

    m_cancel = new QPushButton(QStringLiteral("取消"), this);
    layout->addWidget(m_cancel);

    connect(m_cancel, &QPushButton::clicked, this, &BusyIndicator::cancelRequested);

    setVisible(false);
}

void BusyIndicator::apply(bool busy, bool cancellable, int progress)
{
    setVisible(busy);
    if (!busy) {
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
        return;
    }

    if (progress < 0) {
        m_progress->setRange(0, 0);   // 不确定进度
    } else {
        m_progress->setRange(0, 100);
        m_progress->setValue(progress);
    }
    m_cancel->setVisible(cancellable);
}
