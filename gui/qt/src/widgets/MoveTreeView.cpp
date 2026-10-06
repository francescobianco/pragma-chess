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

/// The link of a move written in a comment: "cm:game:path/basePly/uci,uci",
/// the line from `basePly` of the comment's line up to that move.
QString commentHref(int game, const QList<int> &path, int basePly, const QStringList &uci)
{
    QStringList parts;
    for (const int index : path)
        parts << QString::number(index);
    return QStringLiteral("cm:%1:%2/%3/%4").arg(game).arg(parts.join(QLatin1Char('.'))).arg(basePly).arg(uci.join(QLatin1Char(',')));
}

/// The comment's text as the view shows it, its moves links: `line` is the
/// positions of its line from the game's start, up to its ply `at`.
QString commentHtml(const QString &text, int game, const QList<int> &path, const QList<ChessPosition> &line, int at)
{
    QString html;
    qsizetype done = 0;
    for (const MoveComment::TextMove &move : MoveComment::movesIn(text, line, at)) {
        html += text.mid(done, move.start - done).toHtmlEscaped();
        html += QStringLiteral("<a href=\"%1\" class=\"mv\">%2</a>")
                    .arg(commentHref(game, path, move.basePly, move.uci), text.mid(move.start, move.length).toHtmlEscaped());
        done = move.start + move.length;
    }
    return html + text.mid(done).toHtmlEscaped();
}

/// Is a move written in a comment: its link, or nothing.
bool isCommentLink(const QString &link)
{
    return link.startsWith(QLatin1String("cm:"));
}

/// The text of a move as the view shows it: figurines and annotation symbols.
QString shown(const MoveRecord &move)
{
    return (figurineSan(move.san) + MoveAnnotation::symbols(move.nags)).toHtmlEscaped();
}

/// Room left and right of a paragraph, in pixels: the editor keeps the same.
constexpr int kParagraphPadding = 14;

/// The face of a paragraph of the kind `kind`, from the book's: a title is
/// bold and half as large again, a subtitle bold and a little larger.
QFont kindFont(const QFont &book, Paragraph::Kind kind)
{
    if (kind == Paragraph::Kind::Text)
        return book;
    QFont font = book;
    const qreal scale = kind == Paragraph::Kind::Title ? 1.5 : 1.2;
    if (book.pointSizeF() > 0)
        font.setPointSizeF(book.pointSizeF() * scale);
    else
        font.setPixelSize(qRound(book.pixelSize() * scale));
    font.setWeight(QFont::Bold);
    return font;
}

/// A title or a subtitle sits close to the text it heads: little room under it.
QString headingStyle(Paragraph::Kind kind)
{
    return kind == Paragraph::Kind::Text ? QString() : QStringLiteral(" padding-bottom: 0;");
}

/// A paragraph's text as the view shows it: each line a paragraph of a book,
/// justified, its first line indented, a quarter of a line apart. A title's
/// and a subtitle's lines are centred, not indented.
QString paragraphHtml(const Paragraph &paragraph, const QFont &book)
{
    const QFont font = kindFont(book, paragraph.kind);
    const bool heading = paragraph.kind != Paragraph::Kind::Text;
    // align: the CSS text-align is not honoured here, the attribute is.
    const QString align = heading ? QStringLiteral("center") : QStringLiteral("justify");
    const QString face = heading ? QStringLiteral(" font-size: %1pt; font-weight: 700;").arg(font.pointSizeF()) : QString();
    QString html;
    // An empty paragraph (one just inserted) still has a line to write on.
    const QString &text = paragraph.text;
    for (const QString &line : (text.isEmpty() ? QStringList{QString()} : text.split(QLatin1Char('\n')))) {
        html += QStringLiteral("<p align=\"%1\" style=\"margin: 0; text-indent: %2px; line-height: %3%;%4\">%5</p>")
                    .arg(align)
                    .arg(heading ? 0 : qRound(BookFont::indent(book)))
                    // A heading is one line: the book's leading would only add room under it.
                    .arg(heading ? 100 : BookFont::lineHeight)
                    .arg(face, line.isEmpty() ? QStringLiteral("&nbsp;") : line.toHtmlEscaped());
    }
    return html;
}

/// What makes a paragraph look like a book's in the editor too: `font` is
/// the editor's, already the kind's.
QTextBlockFormat paragraphFormat(const QFont &font, Paragraph::Kind kind)
{
    QTextBlockFormat format;
    format.setTextIndent(kind == Paragraph::Kind::Text ? BookFont::indent(font) : 0);
    format.setAlignment(kind == Paragraph::Kind::Text ? Qt::AlignJustify : Qt::AlignHCenter);
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
    Paragraph::Kind kind = Paragraph::Kind::Text;
    /// A comment: set as the view sets one, plainly, to the left.
    bool comment = false;

    QTextBlockFormat blockFormat() const { return comment ? QTextBlockFormat() : paragraphFormat(font(), kind); }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTextEdit::paintEvent(event);
        // A title or a subtitle starts blank: its place says what it is.
        if (!document()->isEmpty() || hint.isEmpty() || (!comment && kind != Paragraph::Kind::Text))
            return;
        QTextDocument shown;
        shown.setDocumentMargin(document()->documentMargin());
        shown.setDefaultFont(font());
        shown.setPlainText(hint);
        QTextCursor all(&shown);
        all.select(QTextCursor::Document);
        all.mergeBlockFormat(blockFormat());
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
    /// The comment being written, left out here: it has a row of its own.
    QList<int> editedPath;
    int editedComment = -1;

    QString link(const QList<int> &path, int ply, const QString &text) const
    {
        const QString at = href(game, path, ply);
        const bool current = at == currentHref;
        return QStringLiteral("<a href=\"%1\" class=\"%2\">%3</a>").arg(at, current ? QStringLiteral("cur") : QStringLiteral("mv"), text);
    }

    /// A variation and, inline in parentheses, its own: "1…c5 2.Nf3 ( 2.Nc3 ) 2…Nc6".
    /// `line` is the positions from the game's start to the one it starts
    /// from, the ply of its first move minus one.
    void inlineLine(const Variation &variation, const QList<int> &path, QList<ChessPosition> line)
    {
        const int basePly = int(line.size()) - 1;
        bool numbered = false;
        QStringList parts;
        // Comments in a line are set in italics between its moves, their moves links.
        const auto comment = [&](const QString &raw, int index) {
            const QString shownText = MoveComment::displayText(raw);
            if (shownText.isEmpty() || (index == editedComment && path == editedPath))
                return;
            parts << QStringLiteral("<i>%1</i>").arg(commentHtml(shownText, game, path, line, int(line.size()) - 1));
            numbered = false;
        };
        comment(variation.startComment, 0);
        for (qsizetype i = 0; i < variation.moves.size(); ++i) {
            const MoveRecord &move = variation.moves.at(i);
            const int ply = basePly + int(i) + 1;
            const ChessPosition before = line.last();
            QString text = shown(move);
            if (!numbered || before.sideToMove() == Side::White)
                text = before.moveNumberText().toHtmlEscaped() + text;
            numbered = true;
            parts << link(path, ply, text);
            const std::optional<ChessMove> played = before.moveFromUci(move.uci);
            if (!played)
                break;
            line << before;
            line.last().play(*played);
            comment(move.comment, int(i) + 1);
            for (int v = 0; v < variation.variations.size(); ++v) {
                const Variation &inner = variation.variations.at(v);
                if (inner.atPly != i + 1)
                    continue;
                Writer sub{game, currentHref, {}, editedPath, editedComment};
                sub.inlineLine(inner, path + QList<int>{v}, line.first(ply));
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
    connect(header, &QHeaderView::sectionResized, this, [this] {
        if (!m_rebuilding) {
            if (columnWidths() != m_builtWidths)
                rebuild();
            return;
        }
        // Inside setHtml() the new contents move the scroll bar and the
        // columns pass through passing widths: what counts is the width they
        // settle at, once the build is over.
        if (m_widthCheckPending)
            return;
        m_widthCheckPending = true;
        QTimer::singleShot(0, this, [this] {
            m_widthCheckPending = false;
            const QList<int> widths = columnWidths();
            // The width of the build before is the scroll bar going back to
            // where it was: building for it would bring it back, for ever.
            if (widths != m_builtWidths && widths != m_widthsBefore)
                rebuild();
        });
    });
    connect(this, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (isCommentLink(url.toString())) {
            // "cm:game:path/basePly/uci,uci": the line written in the comment.
            const QStringList parts = url.toString().mid(3).split(QLatin1Char('/'));
            if (parts.size() != 3)
                return;
            const Place line = placeOf(parts.at(0) + QLatin1Char('/') + parts.at(1));
            Q_EMIT commentLineActivated(line.game, line.path, line.ply, parts.at(2).split(QLatin1Char(','), Qt::SkipEmptyParts));
            return;
        }
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
    connect(m_session, &GameSession::commentsChanged, this, &MoveTreeView::rebuild);

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
        if (!m_editing.active() || m_formatting)
            return;
        if (isHeading() && m_editor->toPlainText().contains(QLatin1Char('\n'))) {
            // A title is one line: what is pasted on several joins up.
            QString line = m_editor->toPlainText();
            line.replace(QLatin1Char('\n'), QLatin1Char(' '));
            const QSignalBlocker quiet(m_editor);
            m_editor->setPlainText(line);
            m_editor->moveCursor(QTextCursor::End);
        }
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

const GameRecord &MoveTreeView::gameRecord(int game) const
{
    return game == currentGame() || !m_book ? m_session->game() : m_book->chapter().games.at(game).game;
}

MoveTreeView::Place MoveTreeView::placeAt(const QPoint &position) const
{
    const QString anchor = anchorAt(position);
    if (!anchor.isEmpty() && !isCommentLink(anchor))
        return placeOf(anchor);
    Place place;
    const int cell = cellAt(position);
    if (cell < 0)
        return place;
    const int row = cell >> 2;
    if (const auto gameBreak = m_breakRows.constFind(row); gameBreak != m_breakRows.constEnd()) {
        place.game = *gameBreak;
        place.gameBreak = *gameBreak;
        return place;
    }
    if (const auto comment = m_commentRows.constFind(row); comment != m_commentRows.constEnd()) {
        place.game = comment->first;
        place.comment = comment->second;
        return place;
    }
    if (const Place inline_ = inlineCommentAt(position); inline_.isComment())
        return inline_;
    if (const auto start = m_startCells.constFind(cell); start != m_startCells.constEnd()) {
        place.game = *start;
        place.start = true;
        return place;
    }
    if (const auto number = m_numberCells.constFind(row << 2); number != m_numberCells.constEnd() && (cell & 3) == 0) {
        place.game = *number;
        place.moveNumber = true;
        return place;
    }
    if (const auto found = m_cellPlaces.constFind(cell); found != m_cellPlaces.constEnd()) {
        place.game = found->first;
        place.ply = found->second;
    } else if (const auto paragraph = m_paragraphRows.constFind(row); paragraph != m_paragraphRows.constEnd()) {
        place.game = paragraph->first;
        place.paragraph = paragraph->second;
    }
    return place;
}

MoveTreeView::Place MoveTreeView::inlineCommentAt(const QPoint &position) const
{
    // The character under the pointer, if it is a comment's (in italics).
    const QPointF point = QPointF(position) + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const int hit = document()->documentLayout()->hitTest(point, Qt::ExactHit);
    if (hit < 0)
        return {};
    const QTextBlock block = document()->findBlock(hit);
    // The comment follows its move; one before the first move of a line
    // comes after an opening parenthesis, or first in the row.
    QString lastMove;
    QString nextMove;
    bool opened = false;
    bool inComment = false;
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        const QTextCharFormat format = fragment.charFormat();
        const QString link = format.anchorHref();
        const bool comment = format.fontItalic();
        const bool move = !link.isEmpty() && !isCommentLink(link);
        if (hit >= fragment.position() && hit < fragment.position() + fragment.length()) {
            if (!comment)
                return {};
            inComment = true;
        } else if (!inComment && move) {
            lastMove = link;
            opened = false;
        } else if (!inComment && !comment && fragment.text().contains(QLatin1Char('('))) {
            opened = true;
        } else if (inComment && move) {
            nextMove = link;
            break;
        }
    }
    if (!inComment)
        return {};
    Place place;
    if (!lastMove.isEmpty() && !opened) {
        const Place moved = placeOf(lastMove);
        place.game = moved.game;
        place.path = moved.path;
        place.comment = moved.ply - GameVariations::branchPly(gameRecord(moved.game), moved.path);
    } else if (!nextMove.isEmpty()) {
        const Place first = placeOf(nextMove);
        place.game = first.game;
        place.path = first.path;
        place.comment = 0;
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
    // Cells show the hand as links do; paragraphs, which a click leaves
    // alone (a double click writes in them), the arrow.
    if (anchorAt(event->pos()).isEmpty()) {
        const Place place = placeAt(event->pos());
        viewport()->setCursor(place.isMove() || (place.game >= 0 && !place.isParagraph() && !place.isComment())
                                  ? Qt::PointingHandCursor
                                  : Qt::ArrowCursor);
    }
}

void MoveTreeView::mousePressEvent(QMouseEvent *event)
{
    // A click outside the paragraph being written ends the writing first.
    if (m_editing.active())
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
    if (place.isParagraph() || place.isComment())
        return; // Only a double click writes in it.
    if (place.game == currentGame() && place.isMove())
        Q_EMIT moveActivated({}, place.ply);
    else if (place.game == currentGame() && place.start)
        Q_EMIT moveActivated({}, 0);
    else if (place.game >= 0 && place.game != currentGame())
        Q_EMIT gameMoveActivated(place.game, {}, place.ply);
}

void MoveTreeView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const Place place = placeAt(event->pos());
    if (event->button() == Qt::LeftButton && place.isParagraph()) {
        editParagraph(place.game, place.paragraph);
        return;
    }
    if (event->button() == Qt::LeftButton && place.isComment()) {
        // A comment is written in the game on the board: another game comes there first.
        if (place.game != currentGame())
            Q_EMIT gameMoveActivated(place.game, place.path, GameVariations::branchPly(gameRecord(place.game), place.path));
        if (place.game == currentGame())
            editComment(place.path, place.comment);
        return;
    }
    QTextBrowser::mouseDoubleClickEvent(event);
}

bool MoveTreeView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_editor && m_editing.active()) {
        if (event->type() == QEvent::FocusOut) {
            // Not for the editor's own menu, nor for another window coming up front.
            const Qt::FocusReason reason = static_cast<QFocusEvent *>(event)->reason();
            if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason)
                finishEditing();
        } else if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            // Esc, or Ctrl+Enter, ends; a plain Enter starts a new line, but
            // a title or a subtitle is one line: there Enter ends too.
            const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
            if (key->key() == Qt::Key_Escape
                || (enter && (key->modifiers().testFlag(Qt::ControlModifier) || isHeading()))) {
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
    if (m_editing.active())
        finishEditing();
    m_editing = Editing{game, index, {}, -1};
    const Paragraph &paragraph = m_book->chapter().games.at(game).paragraphs.at(index);
    m_editText = paragraph.text;
    // The editor takes the face of what it writes: a title, a subtitle, a paragraph.
    auto *editor = static_cast<ParagraphEditor *>(m_editor);
    editor->kind = paragraph.kind;
    editor->comment = false;
    m_editor->setFont(kindFont(BookFont::paragraph(font()), paragraph.kind));
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

void MoveTreeView::editComment(const QList<int> &path, int index)
{
    const GameRecord &game = m_session->game();
    if (index < 0 || !GameVariations::variationsOf(game, path)
        || (index > 0 && index > GameVariations::lineMoves(game, path).size() - GameVariations::branchPly(game, path)))
        return;
    if (m_editing.active())
        finishEditing();
    m_editing = Editing{currentGame(), -1, path, index};
    m_editText = MoveComment::displayText(MoveComment::at(game, path, index));
    // In the face the comments are shown in: italics, a little smaller.
    auto *editor = static_cast<ParagraphEditor *>(m_editor);
    editor->comment = true;
    QFont face = font();
    face.setItalic(true);
    face.setPointSizeF(face.pointSizeF() * 0.92);
    m_editor->setFont(face);
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

bool MoveTreeView::isHeading() const
{
    const auto *editor = static_cast<const ParagraphEditor *>(m_editor);
    return m_editing.active() && !m_editing.isComment() && editor->kind != Paragraph::Kind::Text;
}

void MoveTreeView::formatEditor()
{
    // Every line a book's paragraph: justified, indented, a quarter of a line
    // apart; or a heading's.
    const QScopedValueRollback<bool> formatting(m_formatting, true);
    QTextCursor all(m_editor->document());
    all.select(QTextCursor::Document);
    all.mergeBlockFormat(static_cast<ParagraphEditor *>(m_editor)->blockFormat());
}

void MoveTreeView::finishEditing()
{
    if (!m_editing.active())
        return;
    const Editing edited = m_editing;
    const QString text = m_editor->toPlainText();
    m_editing = Editing();
    m_editText.clear();
    m_editor->hide();
    if (edited.isComment())
        Q_EMIT commentEdited(edited.path, edited.comment, text);
    else
        Q_EMIT paragraphEdited(edited.game, edited.paragraph, text);
    rebuild();
}

void MoveTreeView::placeEditor()
{
    if (!m_editing.active())
        return;
    QTextTable *table = this->table();
    if (!table || m_editRow < 0)
        return;
    // A paragraph spans the row; a comment leaves the number's column to it.
    const QTextTableCell cell = table->cellAt(m_editRow, m_editing.isComment() ? 1 : 0);
    if (!cell.isValid())
        return;
    // As wide as the list, less its padding, from the text's first line to
    // its last, as it is on screen; a comment from where its text begins.
    const QRect first = cursorRect(cell.firstCursorPosition());
    const QRect last = cursorRect(cell.lastCursorPosition());
    const int left = m_editing.isComment() ? first.left() : kParagraphPadding;
    const int width = viewport()->width() - left - kParagraphPadding;
    m_editor->document()->setTextWidth(width);
    const int height = qMax(last.bottom() - first.top() + 1, int(m_editor->document()->size().height()));
    m_editor->setGeometry(left, first.top(), width, height);
}

QList<int> MoveTreeView::columnWidths() const
{
    return {m_header->sectionSize(0), m_header->sectionSize(1), m_header->sectionSize(2)};
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
    if (const QList<int> widths = columnWidths(); widths != m_builtWidths) {
        m_widthsBefore = m_builtWidths;
        m_builtWidths = widths;
    }
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
                                  "td.start { color: %1; }"
                                  "td.var { font-size: 92%; color: %2; padding-left: 14px; }"
                                  "td.com { font-size: 92%; font-style: italic; color: %3; padding-left: 6px; }"
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
    m_commentRows.clear();
    m_breakRows.clear();
    m_startCells.clear();
    m_numberCells.clear();
    m_editRow = -1;
    m_currentCell = -1;
    int row = 0; // Every row written counts, from 0 (the widths' row).

    const int gameCount = m_book ? int(m_book->chapter().games.size()) : 1;
    for (int g = 0; g < gameCount; ++g) {
        // The game on the board as the session has it; the others as the chapter keeps them.
        const GameRecord &game = g == current ? m_session->game() : m_book->chapter().games.at(g).game;
        QList<Paragraph> paragraphs = m_book ? m_book->chapter().games.at(g).paragraphs : QList<Paragraph>();
        if (m_editing.game == g && !m_editing.isComment() && m_editing.paragraph < paragraphs.size())
            paragraphs[m_editing.paragraph].text = m_editText; // As it is being written.
        // The comment being written, if it is in this game's main line.
        const int editedComment = m_editing.game == g && m_editing.isComment() && m_editing.path.isEmpty()
            ? m_editing.comment
            : -1;

        if (g > 0) {
            // A game break: a light rule across the list, and the numbering starts again.
            ++row;
            m_breakRows.insert(row, g);
            html += QStringLiteral("<tr><td colspan=\"3\" class=\"break\"><hr></td></tr>");
        }

        const int currentPly = g == current && owner.isEmpty() ? m_session->ply() : 0;
        const std::optional<ChessPosition> start = game.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                            : ChessPosition::fromFen(game.startFen, ChessPosition::Kings::Optional);
        ChessPosition position = start.value_or(ChessPosition::startingPosition());
        QList<ChessPosition> line{position}; // The main line's positions so far.
        bool rowOpen = false;
        bool numbered = false; // The game's first move number is written.
        const auto openRow = [&](const QString &number) {
            html += QStringLiteral("<tr><td class=\"n\" width=\"%1\">%2</td>").arg(widths[0], number);
            rowOpen = true;
            ++row;
            if (!numbered) // Its first number: a click there changes it.
                m_numberCells.insert(row << 2, g);
            numbered = true;
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
                if (m_editing.game == g && m_editing.paragraph == p)
                    m_editRow = row;
                html += QStringLiteral("<tr><td colspan=\"3\" class=\"par\" style=\"%1\">%2</td></tr>")
                            .arg(bookStyle + headingStyle(paragraphs.at(p).kind), paragraphHtml(paragraphs.at(p), book));
            }
        };
        // A comment of the main line: a row under its move, in italics,
        // starting where the move does; the moves written in it are links.
        const auto commentRow = [&](const QString &raw, int index) {
            const bool edited = index == editedComment;
            const QString shownText = edited ? m_editText : MoveComment::displayText(raw);
            if (shownText.isEmpty() && !edited)
                return;
            ++row;
            m_commentRows.insert(row, {g, index});
            if (edited)
                m_editRow = row;
            const QString content = edited ? (shownText.isEmpty() ? QStringLiteral("&nbsp;") : shownText.toHtmlEscaped())
                                           : commentHtml(shownText, g, {}, line, index);
            html += QStringLiteral("<tr><td></td><td colspan=\"2\" class=\"com\">%1</td></tr>").arg(content);
        };
        // A comment being written in a variation: a row of its own under the variation's.
        const auto editedVariationRow = [&](int variation) {
            if (m_editing.game != g || !m_editing.isComment() || m_editing.path.value(0, -1) != variation)
                return;
            ++row;
            m_editRow = row;
            html += QStringLiteral("<tr><td></td><td colspan=\"2\" class=\"com\">%1</td></tr>")
                        .arg(m_editText.isEmpty() ? QStringLiteral("&nbsp;") : m_editText.toHtmlEscaped());
        };
        commentRow(game.startComment, 0);
        paragraphRows(0);
        if (game.moves.isEmpty()) {
            // A game with no moves yet still has its place: where its first
            // move will go, faint, the game's own area to click and to delete.
            const bool white = position.sideToMove() == Side::White;
            openRow(QStringLiteral("%1.").arg(position.fullMoveNumber()));
            if (!white)
                html += QStringLiteral("<td class=\"dots\">…</td>");
            const int key = row << 2 | (white ? 1 : 2);
            m_startCells.insert(key, g);
            const bool isCurrent = g == current && m_session->ply() == 0;
            if (isCurrent)
                m_currentCell = key;
            html += QStringLiteral("<td class=\"%1\" width=\"%2\">…</td>")
                        .arg(isCurrent ? QStringLiteral("cur") : QStringLiteral("start"), widths[white ? 1 : 2]);
            closeRow();
        }

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
            if (const std::optional<ChessMove> played = position.moveFromUci(move.uci))
                position.play(*played);
            else
                break;
            line << position;
            // What comes right under the move: its comment, its paragraphs,
            // then the variations that replace it.
            bool hasBlock = !MoveComment::displayText(move.comment).isEmpty() || editedComment == ply;
            for (const Paragraph &paragraph : paragraphs)
                hasBlock = hasBlock || paragraph.ply == ply;
            for (const Variation &variation : game.variations)
                hasBlock = hasBlock || variation.atPly == ply;
            if (!hasBlock)
                continue;
            if (white)
                html += QStringLiteral("<td class=\"dots\">…</td>");
            closeRow();
            commentRow(move.comment, ply);
            paragraphRows(ply);
            for (int v = 0; v < game.variations.size(); ++v) {
                const Variation &variation = game.variations.at(v);
                if (variation.atPly != ply)
                    continue;
                Writer writer{g, currentHref, {}, {}, -1};
                if (m_editing.game == g && m_editing.isComment()) {
                    writer.editedPath = m_editing.path;
                    writer.editedComment = m_editing.comment;
                }
                writer.inlineLine(variation, {v}, line.first(ply));
                html += QStringLiteral("<tr><td></td><td colspan=\"2\" class=\"var\">%1</td></tr>").arg(writer.html);
                ++row;
                editedVariationRow(v);
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
                            .arg(bookStyle + headingStyle(paragraphs.at(p).kind), paragraphHtml(paragraphs.at(p), book));
            }
        }
    }
    html += QStringLiteral("</table>");
    setHtml(html);
    verticalScrollBar()->setValue(scroll);
    if (m_editing.active())
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
