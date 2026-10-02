#include "GlyphMenuAction.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionMenuItem>

namespace {

constexpr int kMargin = 10;
constexpr int kCheckWidth = 12;
constexpr int kGap = 10;

/// The item of one action inside its menu.
class GlyphMenuItem : public QWidget {
public:
    GlyphMenuItem(GlyphMenuAction *action, QMenu *menu)
        : QWidget(menu)
        , m_action(action)
        , m_menu(menu)
    {
        setMouseTracking(true);
        connect(action, &QAction::changed, this, qOverload<>(&QWidget::update));
    }

    QSize sizeHint() const override
    {
        const int text = m_action->symbol().isEmpty()
            ? fontMetrics().horizontalAdvance(m_action->memo())
            : symbolColumnWidth() + kGap + QFontMetrics(memoFont()).horizontalAdvance(m_action->memo());
        const int width = kMargin + kCheckWidth + kGap + text + kMargin;
        // As tall as the style makes the other items.
        const QStyleOptionMenuItem option = styleOption();
        const QSize styled = style()->sizeFromContents(QStyle::CT_MenuItem, &option,
                                                       QSize(width, fontMetrics().height()), m_menu);
        return QSize(width, styled.height());
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const QStyleOptionMenuItem option = styleOption();
        style()->drawControl(QStyle::CE_MenuItem, &option, &painter, m_menu); // The background, highlighted or not.

        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const QPalette::ColorGroup group = m_action->isEnabled() ? QPalette::Normal : QPalette::Disabled;
        const QColor color = palette().color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        painter.setRenderHint(QPainter::Antialiasing);

        int x = kMargin;
        if (m_action->isChecked()) {
            const qreal middle = height() / 2.0;
            QPainterPath check;
            check.moveTo(x + 1.5, middle + 0.5);
            check.lineTo(x + 4.5, middle + 3.5);
            check.lineTo(x + 10.5, middle - 3.5);
            painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(check);
        }
        x += kCheckWidth + kGap;

        painter.setPen(color);
        const QRect line(x, 0, width() - x - kMargin, height());
        if (m_action->symbol().isEmpty()) {
            painter.drawText(line, Qt::AlignVCenter | Qt::AlignLeft, m_action->memo());
            return;
        }
        painter.drawText(line, Qt::AlignVCenter | Qt::AlignLeft, m_action->symbol());
        // The memo is an aside: italic and a little lighter than the symbol.
        QColor aside = color;
        aside.setAlphaF(0.7f);
        painter.setPen(aside);
        painter.setFont(memoFont());
        painter.drawText(line.adjusted(symbolColumnWidth() + kGap, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         m_action->memo());
    }

    // The menu only follows the mouse over its own items: tell it where we are,
    // so that the highlight and the keyboard start from the same item.
    void mouseMoveEvent(QMouseEvent *) override
    {
        if (m_action->isEnabled() && m_menu->activeAction() != m_action)
            m_menu->setActiveAction(m_action);
    }

    void leaveEvent(QEvent *) override
    {
        if (m_menu->activeAction() == m_action)
            m_menu->setActiveAction(nullptr);
        update();
    }

    void mousePressEvent(QMouseEvent *) override {} // Not a click on the menu behind.

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || !m_action->isEnabled() || !rect().contains(event->pos()))
            return;
        // As choosing an item does: the menus go, then the action runs.
        QAction *action = m_action;
        for (QWidget *menu = m_menu; qobject_cast<QMenu *>(menu);) {
            QWidget *parent = menu->parentWidget();
            menu->close();
            menu = parent;
        }
        action->trigger();
    }

private:
    QStyleOptionMenuItem styleOption() const
    {
        QStyleOptionMenuItem option;
        option.initFrom(this);
        option.menuRect = m_menu->rect();
        option.menuItemType = QStyleOptionMenuItem::Normal;
        option.checkType = QStyleOptionMenuItem::NotCheckable;
        option.state.setFlag(QStyle::State_Enabled, m_action->isEnabled());
        option.state.setFlag(QStyle::State_Selected, m_action->isEnabled() && m_menu->activeAction() == m_action);
        return option;
    }

    QFont memoFont() const
    {
        QFont italic = font();
        italic.setItalic(true);
        return italic;
    }

    /// The symbols of a menu share one column, as wide as the widest of them.
    int symbolColumnWidth() const
    {
        int width = 0;
        for (const QAction *action : m_menu->actions()) {
            if (const auto *glyph = qobject_cast<const GlyphMenuAction *>(action))
                width = qMax(width, fontMetrics().horizontalAdvance(glyph->symbol()));
        }
        return width;
    }

    GlyphMenuAction *m_action;
    QMenu *m_menu;
};

} // namespace

GlyphMenuAction::GlyphMenuAction(const QString &symbol, const QString &memo, QObject *parent)
    : QWidgetAction(parent)
    , m_symbol(symbol)
    , m_memo(memo)
{
    // What assistive technologies read.
    setText(symbol.isEmpty() ? memo : symbol + QLatin1Char(' ') + memo);
}

QWidget *GlyphMenuAction::createWidget(QWidget *parent)
{
    auto *menu = qobject_cast<QMenu *>(parent);
    return menu ? new GlyphMenuItem(this, menu) : nullptr;
}
