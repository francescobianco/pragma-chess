#include "MoveTreeView.h"

#include "FigurineFont.h"
#include "app/ChessPosition.h"
#include "app/GameSession.h"
#include "app/GameVariations.h"
#include "app/MoveAnnotation.h"

#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextTable>

namespace {

/// The link of a move: "path/ply", the path's indexes joined by dots.
QString href(const QList<int> &path, int ply)
{
    QStringList parts;
    for (const int index : path)
        parts << QString::number(index);
    return parts.join(QLatin1Char('.')) + QLatin1Char('/') + QString::number(ply);
}

MoveTreeView::Place placeOf(const QString &link)
{
    MoveTreeView::Place place;
    const QStringList halves = link.split(QLatin1Char('/'));
    if (halves.size() != 2)
        return place;
    for (const QString &index : halves.first().split(QLatin1Char('.'), Qt::SkipEmptyParts))
        place.path << index.toInt();
    place.ply = halves.last().toInt();
    return place;
}

/// The text of a move as the view shows it: figurines and annotation symbols.
QString shown(const MoveRecord &move)
{
    return (figurineSan(move.san) + MoveAnnotation::symbols(move.nags)).toHtmlEscaped();
}

struct Writer {
    const GameSession *session;
    QString currentHref;
    QString html;

    QString link(const QList<int> &path, int ply, const QString &text) const
    {
        const QString at = href(path, ply);
        const bool current = at == currentHref;
        return QStringLiteral("<a href=\"%1\" class=\"%2\">%3</a>").arg(at, current ? QStringLiteral("cur") : QStringLiteral("mv"), text);
    }

    /// A variation and, inline in parentheses, its own: "1…c5 2.Nf3 ( 2.Nc3 ) 2…Nc6".
    /// `before` is the position it starts from and `basePly` the ply of its first move, minus one.
    void inlineLine(const Variation &variation, const QList<int> &path, ChessPosition before, int basePly)
    {
        bool numbered = false;
        QStringList parts;
        for (qsizetype i = 0; i < variation.moves.size(); ++i) {
            const MoveRecord &move = variation.moves.at(i);
            const int ply = basePly + int(i) + 1;
            QString text = shown(move);
            if (!numbered || before.sideToMove() == Side::White)
                text = before.moveNumberText().toHtmlEscaped() + text;
            numbered = true;
            parts << link(path, ply, text);
            const ChessPosition at = before;
            if (const std::optional<ChessMove> played = before.moveFromUci(move.uci))
                before.play(*played);
            else
                break;
            for (int v = 0; v < variation.variations.size(); ++v) {
                const Variation &inner = variation.variations.at(v);
                if (inner.atPly != i + 1)
                    continue;
                Writer sub{session, currentHref, {}};
                sub.inlineLine(inner, path + QList<int>{v}, at, ply - 1);
                parts << QStringLiteral("(") + sub.html + QStringLiteral(")");
                numbered = false;
            }
        }
        html += parts.join(QLatin1Char(' '));
    }

    /// The block under a main-line move: each variation on a row of its own,
    /// spanning both columns. Returns the rows written.
    int block(const QList<Variation> &variations, int atPly, const ChessPosition &before)
    {
        int rows = 0;
        for (int v = 0; v < variations.size(); ++v) {
            const Variation &variation = variations.at(v);
            if (variation.atPly != atPly)
                continue;
            Writer line{session, currentHref, {}};
            line.inlineLine(variation, {v}, before, atPly - 1);
            html += QStringLiteral("<tr><td></td><td colspan=\"2\" class=\"var\">%1</td></tr>").arg(line.html);
            ++rows;
        }
        return rows;
    }
};

} // namespace

MoveTreeView::MoveTreeView(GameSession *session, QWidget *parent)
    : QTextBrowser(parent)
    , m_session(session)
{
    setFont(FigurineFont::apply(font()));
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setFocusPolicy(Qt::NoFocus); // The arrows move through the game, not the text.
    setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    document()->setDocumentMargin(0);
    connect(this, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        const Place place = placeOf(url.toString());
        if (place.isValid())
            Q_EMIT moveActivated(place.path, place.ply);
    });
    connect(m_session, &GameSession::gameChanged, this, &MoveTreeView::rebuild);
    connect(m_session, &GameSession::plyChanged, this, &MoveTreeView::rebuild);
    connect(m_session, &GameSession::annotationsChanged, this, &MoveTreeView::rebuild);
    rebuild();
}

MoveTreeView::Place MoveTreeView::placeAt(const QPoint &position) const
{
    const QString anchor = anchorAt(position);
    if (!anchor.isEmpty())
        return placeOf(anchor);
    Place place;
    place.ply = cellPlyAt(position);
    return place;
}

QTextTable *MoveTreeView::table() const
{
    for (QTextFrame *frame : document()->rootFrame()->childFrames()) {
        if (QTextTable *table = qobject_cast<QTextTable *>(frame))
            return table;
    }
    return nullptr;
}

int MoveTreeView::cellPlyAt(const QPoint &position) const
{
    QTextTable *table = this->table();
    if (!table)
        return 0;
    const QTextCursor cursor = cursorForPosition(position);
    const QTextTableCell cell = table->cellAt(cursor);
    if (!cell.isValid())
        return 0;
    return m_cellPlies.value(cell.row() << 2 | cell.column(), 0);
}

void MoveTreeView::mouseMoveEvent(QMouseEvent *event)
{
    QTextBrowser::mouseMoveEvent(event);
    // Cells show the hand as links do.
    if (anchorAt(event->pos()).isEmpty())
        viewport()->setCursor(cellPlyAt(event->pos()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void MoveTreeView::mouseReleaseEvent(QMouseEvent *event)
{
    // A main-line cell is the move: no link to hit, the cell itself goes there.
    const bool onAnchor = !anchorAt(event->pos()).isEmpty();
    QTextBrowser::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton || onAnchor)
        return;
    if (const int ply = cellPlyAt(event->pos()))
        Q_EMIT moveActivated({}, ply);
}

void MoveTreeView::rebuild()
{
    const int scroll = verticalScrollBar()->value();
    const GameRecord &game = m_session->game();
    const QPalette pal = palette();
    const QString highlight = pal.color(QPalette::Highlight).name();
    const QString highlighted = pal.color(QPalette::HighlightedText).name();
    const QString text = pal.color(QPalette::Text).name();
    QColor dim = pal.color(QPalette::Text);
    dim.setAlphaF(0.65f);
    QColor rule = pal.color(QPalette::Mid);

    // The move on the board, by the line that owns it: before the branch, the
    // moves of a variation's line are its parent's.
    QList<int> owner = m_session->path();
    while (!owner.isEmpty() && GameVariations::branchPly(game, owner) >= m_session->ply())
        owner.removeLast();
    Writer writer{m_session, href(owner, m_session->ply()), {}};
    QString &html = writer.html;
    html += QStringLiteral("<style>"
                           "table { border-collapse: collapse; }"
                           "th { font-weight: normal; color: %1; padding: 4px 8px; border-bottom: 1px solid %2; }"
                           "td { padding: 4px 8px; vertical-align: top; }"
                           "td.n { color: %1; text-align: right; }"
                           "td.dots { color: %1; }"
                           "td.cur { color: %5; background-color: %6; }"
                           "td.var { font-size: 92%; color: %3; padding-left: 14px; }"
                           "a { text-decoration: none; }"
                           "a.mv { color: %4; }"
                           "a.cur { color: %5; background-color: %6; }"
                           "</style>")
                .arg(dim.name(QColor::HexArgb), rule.name(), dim.name(QColor::HexArgb), text, highlighted, highlight);
    html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\"><tr><th></th><th width=\"45%\">%1</th><th width=\"45%\">%2</th></tr>")
                .arg(tr("White"), tr("Black"));
    m_cellPlies.clear();
    m_currentCell = -1;
    int row = 0; // The header.
    const int currentPly = owner.isEmpty() ? m_session->ply() : 0;

    const std::optional<ChessPosition> start = game.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                        : ChessPosition::fromFen(game.startFen);
    ChessPosition position = start.value_or(ChessPosition::startingPosition());
    bool rowOpen = false;
    const auto openRow = [&](const QString &number) {
        html += QStringLiteral("<tr><td class=\"n\">%1</td>").arg(number);
        rowOpen = true;
        ++row;
    };
    // A move of the main line fills its cell; the cell is what the user clicks.
    const auto moveCell = [&](int ply, bool white, const MoveRecord &move) {
        const int key = row << 2 | (white ? 1 : 2);
        m_cellPlies.insert(key, ply);
        const bool current = ply == currentPly;
        if (current)
            m_currentCell = key;
        html += QStringLiteral("<td class=\"%1\">%2</td>").arg(current ? QStringLiteral("cur") : QStringLiteral("mv"), shown(move));
    };
    const auto closeRow = [&] {
        if (rowOpen)
            html += QStringLiteral("</tr>");
        rowOpen = false;
    };
    for (qsizetype i = 0; i < game.moves.size(); ++i) {
        const MoveRecord &move = game.moves.at(i);
        const int ply = int(i) + 1;
        const bool white = position.sideToMove() == Side::White;
        const QString number = QStringLiteral("%1.").arg(position.fullMoveNumber());
        if (white) {
            closeRow();
            openRow(number);
        } else if (!rowOpen) {
            openRow(number);
            html += QStringLiteral("<td class=\"dots\">…</td>"); // Black moves first here.
        }
        moveCell(ply, white, move);
        const ChessPosition before = position;
        if (const std::optional<ChessMove> played = position.moveFromUci(move.uci))
            position.play(*played);
        else
            break;
        // The variations that replace this move go right under it.
        bool hasBlock = false;
        for (const Variation &variation : game.variations)
            hasBlock = hasBlock || variation.atPly == ply;
        if (hasBlock) {
            if (white)
                html += QStringLiteral("<td class=\"dots\">…</td>");
            closeRow();
            row += writer.block(game.variations, ply, before);
            if (white) { // Black's move goes on in a row of its own.
                openRow(number);
                html += QStringLiteral("<td class=\"dots\">…</td>");
            }
        }
    }
    closeRow();
    html += QStringLiteral("</table>");
    setHtml(html);
    verticalScrollBar()->setValue(scroll);
    showCurrent();
}

void MoveTreeView::showCurrent()
{
    if (m_currentCell >= 0) {
        if (QTextTable *table = this->table()) {
            const QTextTableCell cell = table->cellAt(m_currentCell >> 2, m_currentCell & 3);
            if (cell.isValid()) {
                setTextCursor(cell.firstCursorPosition());
                ensureCursorVisible();
            }
        }
        return;
    }
    QList<int> owner = m_session->path();
    while (!owner.isEmpty() && GameVariations::branchPly(m_session->game(), owner) >= m_session->ply())
        owner.removeLast();
    const QString current = href(owner, m_session->ply());
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid() && fragment.charFormat().anchorHref() == current) {
                QTextCursor cursor(document());
                cursor.setPosition(fragment.position());
                setTextCursor(cursor);
                ensureCursorVisible();
                return;
            }
        }
    }
}
