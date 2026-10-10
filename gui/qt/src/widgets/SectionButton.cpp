#include "SectionButton.h"

#include <QRegularExpression>
#include <QStyleOptionToolButton>
#include <QStyle>
#include <QStylePainter>

namespace {

/// Room around the text, as the tutor's buttons have it.
constexpr int kPadding = 10;
/// Room around the icon, in its section.
constexpr int kIconPadding = 7;

QString withoutMnemonic(QString text)
{
    return text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
}

} // namespace

SectionButton::SectionButton(QWidget *parent)
    : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // The size of a menu's icons, as the text is a line of the interface.
    const int icon = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    setIconSize(QSize(icon, icon));
}

void SectionButton::setTexts(const QStringList &texts)
{
    m_texts = texts;
    setFixedSize(sizeHint());
}

int SectionButton::sectionWidth() const
{
    return iconSize().width() + 2 * kIconPadding;
}

QSize SectionButton::sizeHint() const
{
    int widest = fontMetrics().horizontalAdvance(withoutMnemonic(text()));
    for (const QString &text : m_texts)
        widest = qMax(widest, fontMetrics().horizontalAdvance(withoutMnemonic(text)));
    const int height = qMax(iconSize().height(), fontMetrics().height()) + 2 * kPadding - kPadding / 2;
    return {sectionWidth() + widest + 2 * kPadding, height};
}

void SectionButton::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionToolButton option;
    initStyleOption(&option);
    const QIcon icon = option.icon;
    const QString text = option.text;
    // The style draws the button alone; the icon and the text are placed here.
    option.icon = QIcon();
    option.text.clear();
    painter.drawComplexControl(QStyle::CC_ToolButton, option);

    const int section = sectionWidth();
    const QRect iconArea(0, 0, section, height());
    const QIcon::Mode mode = isEnabled() ? QIcon::Normal : QIcon::Disabled;
    QRect iconRect(QPoint(), iconSize());
    iconRect.moveCenter(iconArea.center());
    icon.paint(&painter, iconRect, Qt::AlignCenter, mode, isChecked() ? QIcon::On : QIcon::Off);

    // The line that sets the icon's section off, faint, inside the frame.
    QColor line = palette().color(QPalette::ButtonText);
    line.setAlphaF(0.18);
    painter.setPen(line);
    const int margin = height() / 4;
    painter.drawLine(section, margin, section, height() - margin);

    painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
    painter.drawText(QRect(section, 0, width() - section, height()), Qt::AlignCenter | Qt::TextHideMnemonic, text);
}
