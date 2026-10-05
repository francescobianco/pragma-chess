#include "MoveTreeView.h"

#include "BookFont.h"
#include "FigurineFont.h"
#include "PaddedHeaderView.h"
#include "PaddedItemDelegate.h"
#include "app/Chapters.h"
#include "app/ChessPosition.h"
#include "app/GameSession.h"
#include "app/GameVariations.h"
#include "app/MoveAnnotation.h"
#include "app/MoveComment.h"

#include <QAbstractTextDocumentLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextTable>
#include <QTimer>
#include <QScopedValueRollback>

namespace {

/// The link of a move: "game:path/ply", the path's indexes joined by dots.
QString href(int game, const QList<int> &path, int ply)
{
    QStringList parts;
    for (const int index : path)
        parts << QString::number(index);
    return QStringLiteral("%1:%2/%3").arg(game).arg(parts.join(QLatin1Char('.'))).arg(ply);
}

MoveTreeView::Place placeOf(const QString &link)
{
    MoveTreeView::Place place;
    const int colon = int(link.indexOf(QLatin1Char(':')));
    const QStringList halves = link.mid(colon + 1).split(QLatin1Char('/'));
    if (colon < 0 || halves.size() != 2)
        return place;
    place.game = link.left(colon).toInt();
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

/// Room left and right of a paragraph, in pixels: the editor keeps the same.
constexpr int kParagraphPadding = 14;

/// A paragraph's text as the view shows it: each line a paragraph of a book,
/// justified, its first line indented, a quarter of a line apart.
QString paragraphHtml(const QString &text, qreal indent)
{
    QString html;
    // An empty paragraph (one just inserted) still has a line to write on.
    for (const QString &line : (text.isEmpty() ? QStringList{QString()} : text.split(QLatin1Char('\n')))) {
        // align="justify": the CSS text-align is not honoured here, the attribute is.
        html += QStringLiteral("<p align=\"justify\" style=\"margin: 0; text-indent: %1px; line-height: %2%;\">%3</p>")
                    .arg(qRound(indent))
                    .arg(BookFont::lineHeight)
                    .arg(line.isEmpty() ? QStringLiteral("&nbsp;") : line.toHtmlEscaped());
    }
    return html;
}

/// What makes a paragraph look like a book's in the editor too.
QTextBlockFormat paragraphFormat(qreal indent)
{
    QTextBlockFormat format;
    format.setTextIndent(indent);
    format.setAlignment(Qt::AlignJustify);
    format.setLineHeight(BookFont::lineHeight, QTextBlockFormat::ProportionalHeight);
    return format;
}

/// The editor of a paragraph. Its hint, while it is empty, is set as the
/// paragraph will be — indented, justified —: QTextEdit's own placeholder
/// would sit at the left edge, where the text does not begin.
class ParagraphEditor : public QTextEdit {
public:
    using QTextEdit::QTextEdit;

    QString hint;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTextEdit::paintEvent(event);
        if (!document()->isEmpty() || hint.isEmpty())
            return;
        QTextDocument shown;
        shown.setDocumentMargin(document()->documentMargin());
        shown.setDefaultFont(font());
        shown.setPlainText(hint);
        QTextCursor all(&shown);
        all.select(QTextCursor::Document);
        all.mergeBlockFormat(paragraphFormat(BookFont::indent(font())));
        shown.setTextWidth(viewport()->width());
        QPainter painter(viewport());
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = palette();
        context.palette.setColor(QPalette::Text, palette().color(QPalette::PlaceholderText));
        shown.documentLayout()->draw(&painter, context);
    }
};

struct Writer {
    int game = 0;
    QString currentHref;
    QString html;

    QString link(const QList<int> &path, int ply, const QString &text) const
    {
        const QString at = href(game, path, ply);
        const bool current = at == currentHref;
        return QStringLiteral("<a href=\"%1\" class=\"%2\">%3</a>").arg(at, current ? QStringLiteral("cur") : QStringLiteral("mv"), text);
    }

    /// A variation and, inline in parentheses, its own: "1…c5 2.Nf3 ( 2.Nc3 ) 2…Nc6".
    /// `before` is the position it starts from and `basePly` the ply of its first move, minus one.
    void inlineLine(const Variation &variation, const QList<int> &path, ChessPosition before, int basePly)
    {
        bool numbered = false;
        QStringList parts;
        // Comments in a line are set in italics between its moves.
        const auto comment = [&parts, &numbered](const QString &raw) {
            const QString shownText = MoveComment::displayText(raw);
            if (shownText.isEmpty())
                return;
            parts << QStringLiteral("<i>%1</i>").arg(shownText.toHtmlEscaped());
            numbered = false;
        };
        comment(variation.startComment);
        for (qsizetype i = 0; i < variation.moves.size(); ++i) {
            const MoveRecord &move = variation.moves.at(i);
            const int ply = basePly + int(i) + 1;
            QString text = shown(move);
            if (!numbered || before.sideToMove() == Side::White)
                text = before.moveNumberText().toHtmlEscaped() + text;
            numbered = true;
            parts << link(path, ply, text);
            comment(move.comment);
            const ChessPosition at = before;
            if (const std::optional<ChessMove> played = before.moveFromUci(move.uci))
                before.play(*played);
            else
                break;
            for (int v = 0; v < variation.variations.size(); ++v) {
                const Variation &inner = variation.variations.at(v);
                if (inner.atPly != i + 1)
                    continue;
                Writer sub{game, currentHref, {}};
                sub.inlineLine(inner, path + QList<int>{v}, at, ply - 1);
                parts << QStringLiteral("(") + sub.html + QStringLiteral(")");
                numbered = false;
            }
        }
        html += parts.join(QLatin1Char(' '));
    }
};

} // namespace

MoveTreeView::MoveTreeView(GameSession *session, QWidget *parent)
    : QTextBrowser(parent)
    , m_session(session)
    , m_editor(new ParagraphEditor(viewport()))
{
    setFont(FigurineFont::apply(font()));
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setFocusPolicy(Qt::NoFocus); // The arrows move through the game, not the text.
    setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    // The same frame as the other tables, or its header sits a pixel higher than theirs.
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // The table is as wide as the view, whatever its padding adds.
    setMouseTracking(true);
    document()->setDocumentMargin(0);

    // The header: number, White, Black; the number column fits "199." and
    // the others share the rest.
    m_columns = new QStandardItemModel(0, 3, this);
    m_columns->setHorizontalHeaderLabels({QString(), tr("White"), tr("Black")});
    auto *header = new PaddedHeaderView(Qt::Horizontal, CellPadding::vertical, CellPadding::horizontal, this);
    m_header = header;
    header->setModel(m_columns);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionsClickable(false);
    header->setSectionsMovable(false);
    header->setHighlightSections(false);
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, fontMetrics().horizontalAdvance(QStringLiteral("199.")) + 2 * CellPadding::horizontal);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    setViewportMargins(0, header->sizeHint().height(), 0, 0);
    connect(header, &QHeaderView::sectionResized, this, [this] { rebuild(); });
    connect(this, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        const Place place = placeOf(url.toString());
        if (!place.isMove())
            return;
        if (place.game == currentGame())
            Q_EMIT moveActivated(place.path, place.ply);
        else
            Q_EMIT gameMoveActivated(place.game, place.path, place.ply);
    });
    connect(m_session, &GameSession::gameChanged, this, &MoveTreeView::rebuild);
    connect(m_session, &GameSession::plyChanged, this, &MoveTreeView::rebuild);
    connect(m_session, &GameSession::annotationsChanged, this, &MoveTreeView::rebuild);

    // Paragraphs are written where they are: the editor lies over the
    // paragraph's row, in the same font, and the row grows with the text.
    m_editor->setFont(BookFont::paragraph(font()));
    m_editor->setAcceptRichText(false);
    m_editor->setFrameShape(QFrame::NoFrame);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->document()->setDocumentMargin(0);
    static_cast<ParagraphEditor *>(m_editor)->hint = tr("Write here; Esc or a click elsewhere ends");
    m_editor->hide();
    m_editor->installEventFilter(this);
    connect(m_editor, &QTextEdit::textChanged, this, [this] {
        if (m_editing.first < 0 || m_formatting)
            return;
        formatEditor();
        m_editText = m_editor->toPlainText();
        rebuild();
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &MoveTreeView::placeEditor);
    rebuild();
}

void MoveTreeView::setBook(const ChapterBook *book)
{
    m_book = book;
    rebuild();
}

void MoveTreeView::refresh()
{
    rebuild();
}

int MoveTreeView::currentGame() const
{
    return m_book ? m_book->chapter().currentGame : 0;
}

MoveTreeView::Place MoveTreeView::placeAt(const QPoint &position) const
{
    const QString anchor = anchorAt(position);
    if (!anchor.isEmpty())
        return placeOf(anchor);
    Place place;
    const int cell = cellAt(position);
    if (cell < 0)
        return place;
    const int row = cell >> 2;
    if (const auto found = m_cellPlaces.constFind(cell); found != m_cellPlaces.constEnd()) {
        place.game = found->first;
        place.ply = found->second;
    } else if (const auto paragraph = m_paragraphRows.constFind(row); paragraph != m_paragraphRows.constEnd()) {
        place.game = paragraph->first;
        place.paragraph = paragraph->second;
    }
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

int MoveTreeView::cellAt(const QPoint &position) const
{
    QTextTable *table = this->table();
    if (!table)
        return -1;
    const QTextCursor cursor = cursorForPosition(position);
    const QTextTableCell cell = table->cellAt(cursor);
    if (!cell.isValid())
        return -1;
    // A cell spanning columns answers for all of them: the row is what counts.
    return cell.row() << 2 | cell.column();
}

void MoveTreeView::resizeEvent(QResizeEvent *event)
{
    QTextBrowser::resizeEvent(event);
    const int frame = frameWidth();
    m_header->setGeometry(frame, frame, viewport()->width(), m_header->sizeHint().height());
    placeEditor();
}

void MoveTreeView::mouseMoveEvent(QMouseEvent *event)
{
    QTextBrowser::mouseMoveEvent(event);
    // Cells show the hand as links do; paragraphs the text cursor.
    if (anchorAt(event->pos()).isEmpty()) {
        const Place place = placeAt(event->pos());
        viewport()->setCursor(place.isParagraph() ? Qt::IBeamCursor
                              : place.game >= 0  ? Qt::PointingHandCursor
                                                 : Qt::ArrowCursor);
    }
}

void MoveTreeView::mousePressEvent(QMouseEvent *event)
{
    // A click outside the paragraph being written ends the writing first.
    if (m_editing.first >= 0)
        finishEditing();
    QTextBrowser::mousePressEvent(event);
}

void MoveTreeView::mouseReleaseEvent(QMouseEvent *event)
{
    // A main-line cell is the move: no link to hit, the cell itself goes there.
    const bool onAnchor = !anchorAt(event->pos()).isEmpty();
    QTextBrowser::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton || onAnchor)
        return;
    const Place place = placeAt(event->pos());
    if (place.isParagraph())
        editParagraph(place.game, place.paragraph);
    else if (place.game == currentGame() && place.isMove())
        Q_EMIT moveActivated({}, place.ply);
    else if (place.game >= 0 && place.game != currentGame())
        Q_EMIT gameMoveActivated(place.game, {}, place.ply);
}

bool MoveTreeView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_editor && m_editing.first >= 0) {
        if (event->type() == QEvent::FocusOut) {
            // Not for the editor's own menu, nor for another window coming up front.
            const Qt::FocusReason reason = static_cast<QFocusEvent *>(event)->reason();
            if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason)
                finishEditing();
        } else if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            // Esc, or Ctrl+Enter, ends; a plain Enter starts a new line.
            if (key->key() == Qt::Key_Escape
                || ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
                    && key->modifiers().testFlag(Qt::ControlModifier))) {
                finishEditing();
                return true;
            }
        }
    }
    return QTextBrowser::eventFilter(watched, event);
}

void MoveTreeView::editParagraph(int game, int index)
{
    if (!m_book || game < 0 || game >= m_book->chapter().games.size()
        || index < 0 || index >= m_book->chapter().games.at(game).paragraphs.size())
        return;
    if (m_editing.first >= 0)
        finishEditing();
    m_editing = {game, index};
    m_editText = m_book->chapter().games.at(game).paragraphs.at(index).text;
    {
        const QSignalBlocker quiet(m_editor);
        m_editor->setPlainText(m_editText);
        formatEditor();
    }
    m_editor->moveCursor(QTextCursor::End);
    rebuild();
    m_editor->show();
    m_editor->setFocus();
}

void MoveTreeView::formatEditor()
{
    // Every line a book's paragraph: justified, indented, a quarter of a line apart.
    const QScopedValueRollback<bool> formatting(m_formatting, true);
    QTextCursor all(m_editor->document());
    all.select(QTextCursor::Document);
    all.mergeBlockFormat(paragraphFormat(BookFont::indent(m_editor->font())));
}

void MoveTreeView::finishEditing()
{
    if (m_editing.first < 0)
        return;
    const QPair<int, int> edited = m_editing;
    const QString text = m_editor->toPlainText();
    m_editing = {-1, -1};
    m_editText.clear();
    m_editor->hide();
    Q_EMIT paragraphEdited(edited.first, edited.second, text);
    rebuild();
}

void MoveTreeView::placeEditor()
{
    if (m_editing.first < 0)
        return;
    QTextTable *table = this->table();
    int row = -1;
    for (auto it = m_paragraphRows.constBegin(); it != m_paragraphRows.constEnd(); ++it) {
        if (it.value() == m_editing)
            row = it.key();
    }
    if (!table || row < 0)
        return;
    const QTextTableCell cell = table->cellAt(row, 0); // The paragraph spans the row.
    if (!cell.isValid())
        return;
    // As wide as the list, less its padding, from the paragraph's first line
    // to its last, as it is on screen.
    const QRect first = cursorRect(cell.firstCursorPosition());
    const QRect last = cursorRect(cell.lastCursorPosition());
    const int width = viewport()->width() - 2 * kParagraphPadding;
    m_editor->document()->setTextWidth(width);
    const int height = qMax(last.bottom() - first.top() + 1, int(m_editor->document()->size().height()));
    m_editor->setGeometry(kParagraphPadding, first.top(), width, height);
}

void MoveTreeView::rebuild()
{
    if (m_rebuilding) {
        // Asked from inside setHtml(): a second document would be built into the first.
        if (!m_rebuildAgain)
            QTimer::singleShot(0, this, [this] {
                m_rebuildAgain = false;
                rebuild();
            });
        m_rebuildAgain = true;
        return;
    }
    const QScopedValueRollback<bool> running(m_rebuilding, true);
    const int scroll = verticalScrollBar()->value();
    // The rule between games is drawn with the palette's Dark: a light one,
    // a little of the text over the page.
    {
        QPalette rule = palette();
        const QColor base = rule.color(QPalette::Base);
        const QColor ink = rule.color(QPalette::Text);
        const auto mix = [&](int a, int b) { return a + (b - a) * 18 / 100; };
        const QColor light(mix(base.red(), ink.red()), mix(base.green(), ink.green()), mix(base.blue(), ink.blue()));
        if (rule.color(QPalette::Dark) != light) {
            rule.setColor(QPalette::Dark, light);
            setPalette(rule);
        }
    }
    const QPalette pal = palette();
    const QString highlight = pal.color(QPalette::Highlight).name();
    const QString highlighted = pal.color(QPalette::HighlightedText).name();
    const QString text = pal.color(QPalette::Text).name();
    QColor dim = pal.color(QPalette::Text);
    dim.setAlphaF(0.65f);

    const int current = currentGame();
    // The move on the board, by the line that owns it: before the branch, the
    // moves of a variation's line are its parent's.
    QList<int> owner = m_session->path();
    while (!owner.isEmpty() && GameVariations::branchPly(m_session->game(), owner) >= m_session->ply())
        owner.removeLast();
    const QString currentHref = href(current, owner, m_session->ply());

    QString html = QStringLiteral("<style>"
                                  "table { border-collapse: collapse; }"
                                  "td { padding: 4px 8px; vertical-align: top; }"
                                  "td.n { color: %1; text-align: right; }"
                                  "td.dots { color: %1; }"
                                  "td.cur { color: %4; background-color: %5; }"
                                  "td.var { font-size: 92%; color: %2; padding-left: 14px; }"
                                  "td.com { font-size: 92%; font-style: italic; color: %3; padding-left: 14px; }"
                                  "td.par { color: %3; padding: 8px %6px; }"
                                  "td.break { padding: 10px %6px; }"
                                  "a { text-decoration: none; }"
                                  "a.mv { color: %3; }"
                                  "a.cur { color: %4; background-color: %5; }"
                                  "</style>")
                       .arg(dim.name(QColor::HexArgb), dim.name(QColor::HexArgb), text, highlighted, highlight)
                       .arg(kParagraphPadding);
    // Paragraphs are set as a book's, in their own face.
    const QFont book = BookFont::paragraph(font());
    const qreal indent = BookFont::indent(book);
    const QString bookStyle = QStringLiteral("font-family: '%1'; font-size: %2pt; font-weight: 200;")
                                  .arg(book.families().join(QStringLiteral("', '")))
                                  .arg(book.pointSizeF());
    // The columns are as wide as the header's sections, so the two line up.
    const QString widths[3] = {QString::number(m_header->sectionSize(0)), QString::number(m_header->sectionSize(1)),
                               QString::number(m_header->sectionSize(2))};
    html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\">");
    // A row of nothing that holds the columns' widths: rows spanning them
    // (paragraphs, game breaks) would otherwise have the table share them out anew.
    html += QStringLiteral("<tr style=\"font-size: 1px;\"><td width=\"%1\" style=\"padding: 0;\"></td>"
                           "<td width=\"%2\" style=\"padding: 0;\"></td><td width=\"%3\" style=\"padding: 0;\"></td></tr>")
                .arg(widths[0], widths[1], widths[2]);
    m_cellPlaces.clear();
    m_paragraphRows.clear();
    m_currentCell = -1;
    int row = 0; // Every row written counts, from 0 (the widths' row).

    const int gameCount = m_book ? int(m_book->chapter().games.size()) : 1;
    for (int g = 0; g < gameCount; ++g) {
        // The game on the board as the session has it; the others as the chapter keeps them.
        const GameRecord &game = g == current ? m_session->game() : m_book->chapter().games.at(g).game;
        QList<Paragraph> paragraphs = m_book ? m_book->chapter().games.at(g).paragraphs : QList<Paragraph>();
        if (m_editing.first == g && m_editing.second < paragraphs.size())
            paragraphs[m_editing.second].text = m_editText; // As it is being written.

        if (g > 0) {
            // A game break: a light rule across the list, and the numbering starts again.
            ++row;
            html += QStringLiteral("<tr><td colspan=\"3\" class=\"break\"><hr></td></tr>");
        }

        const int currentPly = g == current && owner.isEmpty() ? m_session->ply() : 0;
        const std::optional<ChessPosition> start = game.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                            : ChessPosition::fromFen(game.startFen);
        ChessPosition position = start.value_or(ChessPosition::startingPosition());
        bool rowOpen = false;
        const auto openRow = [&](const QString &number) {
            html += QStringLiteral("<tr><td class=\"n\" width=\"%1\">%2</td>").arg(widths[0], number);
            rowOpen = true;
            ++row;
        };
        // A move of the main line fills its cell; the cell is what the user clicks.
        const auto moveCell = [&](int ply, bool white, const MoveRecord &move) {
            const int key = row << 2 | (white ? 1 : 2);
            m_cellPlaces.insert(key, {g, ply});
            const bool isCurrent = ply == currentPly;
            if (isCurrent)
                m_currentCell = key;
            html += QStringLiteral("<td class=\"%1\" width=\"%2\">%3</td>")
                        .arg(isCurrent ? QStringLiteral("cur") : QStringLiteral("mv"), widths[white ? 1 : 2], shown(move));
        };
        const auto closeRow = [&] {
            if (rowOpen)
                html += QStringLiteral("</tr>");
            rowOpen = false;
        };
        // The paragraphs after `ply`, each a row of its own.
        const auto paragraphRows = [&](int ply) {
            for (int p = 0; p < paragraphs.size(); ++p) {
                if (paragraphs.at(p).ply != ply)
                    continue;
                ++row;
                m_paragraphRows.insert(row, {g, p});
                html += QStringLiteral("<tr><td colspan=\"3\" class=\"par\" style=\"%1\">%2</td></tr>")
                            .arg(bookStyle, paragraphHtml(paragraphs.at(p).text, indent));
            }
        };
        // A comment of the main line: a row under its move, in italics.
        const auto commentRow = [&](const QString &raw) {
            const QString shownText = MoveComment::displayText(raw);
            if (shownText.isEmpty())
                return;
            ++row;
            html += QStringLiteral("<tr><td></td><td colspan=\"2\" class=\"com\">%1</td></tr>")
                        .arg(shownText.toHtmlEscaped());
        };
        commentRow(game.startComment);
        paragraphRows(0);

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
            // What comes right under the move: its comment, its paragraphs,
            // then the variations that replace it.
            bool hasBlock = !MoveComment::displayText(move.comment).isEmpty();
            for (const Paragraph &paragraph : paragraphs)
                hasBlock = hasBlock || paragraph.ply == ply;
            for (const Variation &variation : game.variations)
                hasBlock = hasBlock || variation.atPly == ply;
            if (!hasBlock)
                continue;
            if (white)
                html += QStringLiteral("<td class=\"dots\">…</td>");
            closeRow();
            commentRow(move.comment);
            paragraphRows(ply);
            for (int v = 0; v < game.variations.size(); ++v) {
                const Variation &variation = game.variations.at(v);
                if (variation.atPly != ply)
                    continue;
                Writer line{g, currentHref, {}};
                line.inlineLine(variation, {v}, before, ply - 1);
                html += QStringLiteral("<tr><td></td><td colspan=\"2\" class=\"var\">%1</td></tr>").arg(line.html);
                ++row;
            }
            if (white) { // Black's move goes on in a row of its own.
                openRow(number);
                html += QStringLiteral("<td class=\"dots\">…</td>");
            }
        }
        closeRow();
        // Paragraphs anchored past the last move (moves cut since) still show, at the end.
        for (int p = 0; p < paragraphs.size(); ++p) {
            if (paragraphs.at(p).ply > game.moves.size()) {
                ++row;
                m_paragraphRows.insert(row, {g, p});
                html += QStringLiteral("<tr><td colspan=\"3\" class=\"par\" style=\"%1\">%2</td></tr>")
                            .arg(bookStyle, paragraphHtml(paragraphs.at(p).text, indent));
            }
        }
    }
    html += QStringLiteral("</table>");
    setHtml(html);
    verticalScrollBar()->setValue(scroll);
    if (m_editing.first >= 0)
        placeEditor(); // The writer's place, not the move's.
    else
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
    const QString current = href(currentGame(), owner, m_session->ply());
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
