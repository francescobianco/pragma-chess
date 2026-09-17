#include "GameHeaderWidget.h"

#include <QEnterEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace {

bool isUnknown(const QString &value)
{
    const QString v = value.trimmed();
    return v.isEmpty() || v == QLatin1String("?") || v == QLatin1String("-");
}

/// "Adolf" → "A.", "Jean-Pierre" → "J.-P.", "Magnus Øen" → "M."
QString initials(const QString &givenNames)
{
    const QString first = givenNames.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0);
    QStringList parts;
    for (const QString &part : first.split(QLatin1Char('-'), Qt::SkipEmptyParts)) {
        // Already an initial ("A." or "A") stays as it is.
        parts << part.left(1).toUpper() + QLatin1Char('.');
    }
    return parts.join(QLatin1Char('-'));
}

} // namespace

QString GameHeaderWidget::playerNameHtml(const QString &pgnName)
{
    if (isUnknown(pgnName))
        return QStringLiteral("<b>?</b>");

    // PGN convention: "Surname, Given names". Anything else is shown as is.
    const qsizetype comma = pgnName.indexOf(QLatin1Char(','));
    if (comma < 0)
        return QStringLiteral("<b>%1</b>").arg(pgnName.trimmed().toHtmlEscaped());

    const QString surname = pgnName.left(comma).trimmed();
    const QString given = initials(pgnName.mid(comma + 1));
    if (given.isEmpty())
        return QStringLiteral("<b>%1</b>").arg(surname.toHtmlEscaped());
    return QStringLiteral("%1&nbsp;<b>%2</b>").arg(given.toHtmlEscaped(), surname.toHtmlEscaped());
}

GameHeaderWidget::GameHeaderWidget(QWidget *parent)
    : QWidget(parent)
    , m_players(new QLabel)
    , m_details(new QLabel)
{
    setAccessibleName(tr("Game information"));
    setAttribute(Qt::WA_Hover);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(1);
    m_details->setTextFormat(Qt::PlainText);
    m_players->setTextFormat(Qt::RichText);
    for (QLabel *label : {m_players, m_details}) {
        label->setAlignment(Qt::AlignCenter);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(label);
    }
    updateFonts();
}

void GameHeaderWidget::updateFonts()
{
    fitPlayersText();

    QFont details = font();
    details.setItalic(true);
    m_details->setFont(details);
    QPalette palette = m_details->palette();
    palette.setColor(QPalette::WindowText, palette.color(QPalette::PlaceholderText));
    m_details->setPalette(palette);
}

void GameHeaderWidget::setGame(const GameRecord &game, bool editable)
{
    m_editable = editable;
    setCursor(editable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setFocusPolicy(editable ? Qt::TabFocus : Qt::NoFocus);
    setToolTip(editable ? tr("Click to edit the game information") : QString());

    const bool hasPlayers = !isUnknown(game.white) || !isUnknown(game.black);
    if (!hasPlayers && game.moves.isEmpty() && !editable) {
        m_players->setText(QStringLiteral("<b>%1</b>").arg(tr("Position").toHtmlEscaped()));
    } else {
        const QString versus = QStringLiteral("&nbsp;&nbsp;<span style=\"font-style:italic; font-weight:normal;\">%1</span>&nbsp;&nbsp;")
                                   .arg(tr("vs").toHtmlEscaped());
        m_players->setText(playerNameHtml(game.white) + versus + playerNameHtml(game.black));
    }

    QStringList details;
    static const QRegularExpression year(QStringLiteral("^(\\d{4})"));
    const QRegularExpressionMatch match = year.match(game.date);
    if (match.hasMatch())
        details << match.captured(1);
    if (!isUnknown(game.event))
        details << game.event.trimmed();
    m_detailsText = details.isEmpty() && editable ? tr("Add year and tournament")
                                                  : details.join(QStringLiteral(" · "));
    m_details->setVisible(!m_detailsText.isEmpty());
    fitPlayersText(); // After the details text, which it also lays out.

    setAccessibleDescription(QStringLiteral("%1 %2 %3. %4")
                                 .arg(game.white, tr("vs"), game.black, details.join(QStringLiteral(", "))));
    update();
}

void GameHeaderWidget::fitPlayersText()
{
    const int available = width() - layout()->contentsMargins().left() - layout()->contentsMargins().right();
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setHtml(m_players->text());

    // Largest size between 1.3× and 0.6× the base font that fits the width.
    QFont players = font();
    for (qreal scale = 1.3; scale >= 0.6; scale -= 0.05) {
        players.setPointSizeF(font().pointSizeF() * scale);
        document.setDefaultFont(players);
        if (available <= 0 || document.idealWidth() <= available)
            break;
    }
    m_players->setFont(players);

    // The details line keeps its size and is elided instead.
    m_details->setText(m_details->fontMetrics().elidedText(m_detailsText, Qt::ElideRight, qMax(0, available)));
}

void GameHeaderWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    fitPlayersText();
}

void GameHeaderWidget::paintEvent(QPaintEvent *)
{
    if (!m_editable || (!m_hovered && !hasFocus()))
        return;
    // Subtle rounded highlight, like a flat button on hover.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor fill = palette().color(QPalette::WindowText);
    fill.setAlphaF(m_hovered ? 0.08 : 0.05);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
    painter.fillPath(path, fill);
    if (hasFocus()) {
        QColor ring = palette().color(QPalette::Highlight);
        painter.setPen(QPen(ring, 1.5));
        painter.drawPath(path);
    }
}

void GameHeaderWidget::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void GameHeaderWidget::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void GameHeaderWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_editable && event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        Q_EMIT activated();
    QWidget::mouseReleaseEvent(event);
}

void GameHeaderWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_editable && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
                       || event->key() == Qt::Key_Space)) {
        Q_EMIT activated();
        return;
    }
    QWidget::keyPressEvent(event);
}

void GameHeaderWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange)
        updateFonts();
    QWidget::changeEvent(event);
}
