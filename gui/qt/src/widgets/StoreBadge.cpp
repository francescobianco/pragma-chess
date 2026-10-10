#include "StoreBadge.h"

#include <QDesktopServices>
#include <QPainter>
#include <QPainterPath>

StoreBadge::StoreBadge(const QString &caption, const QString &name, const QUrl &url, QWidget *parent)
    : QAbstractButton(parent)
    , m_caption(caption)
    , m_name(name)
    , m_url(url)
{
    setEnabled(url.isValid());
    setCursor(url.isValid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setAccessibleName(caption + QLatin1Char(' ') + name);
    if (url.isValid())
        setToolTip(url.toString());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(this, &QAbstractButton::clicked, this, [this] { QDesktopServices::openUrl(m_url); });
}

QSize StoreBadge::sizeHint() const
{
    return {136, 46};
}

QSize StoreBadge::minimumSizeHint() const
{
    return {104, 46};
}

void StoreBadge::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        painter.setOpacity(0.38); // Coming soon: there, but not yet.
    const QRectF tile = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor(0xa6, 0xa6, 0xa6), 1));
    painter.setBrush(underMouse() && isEnabled() ? QColor(0x22, 0x22, 0x22) : QColor(0x0b, 0x0b, 0x0b));
    painter.drawRoundedRect(tile, 7, 7);

    painter.setPen(Qt::white);
    QFont caption = font();
    caption.setPixelSize(10);
    QFont name = font();
    name.setPixelSize(16);
    name.setWeight(QFont::DemiBold);
    const QRectF text = tile.adjusted(10, 5, -8, -5);
    painter.setFont(caption);
    painter.drawText(QRectF(text.left(), text.top(), text.width(), text.height() * 0.38),
                     Qt::AlignLeft | Qt::AlignBottom, m_caption);
    painter.setFont(name);
    const QString shown = QFontMetrics(name).elidedText(m_name, Qt::ElideRight, int(text.width()));
    painter.drawText(QRectF(text.left(), text.top() + text.height() * 0.38, text.width(), text.height() * 0.62),
                     Qt::AlignLeft | Qt::AlignVCenter, shown);
}
