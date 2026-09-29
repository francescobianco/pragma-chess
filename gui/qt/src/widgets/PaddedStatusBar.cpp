#include "PaddedStatusBar.h"

#include <QLabel>
#include <QPainter>
#include <QStyleOption>

namespace {

/// Room between the text and the window's side edges: clear of macOS's rounded corners.
constexpr int kSideMargin = 12;

} // namespace

PaddedStatusBar::PaddedStatusBar(QWidget *parent)
    : QStatusBar(parent)
    , m_message(new QLabel(this))
{
    setContentsMargins(kSideMargin, 0, kSideMargin, 0);
    // Permanent, so a message does not hide it; stretching, so it takes the left side.
    m_message->setTextFormat(Qt::PlainText);
    m_message->setMinimumWidth(0);
    m_message->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    addPermanentWidget(m_message, 1);
    connect(this, &QStatusBar::messageChanged, m_message, &QLabel::setText);
}

void PaddedStatusBar::paintEvent(QPaintEvent *)
{
    // The panel only: the message is drawn by the label.
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_PanelStatusBar, &option, &painter, this);
}
