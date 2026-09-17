#include "BoardWidget.h"

#include "PieceRenderer.h"

#include <QApplication>
#include <QFocusEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QSet>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <cmath>

#include <cmath>


namespace {

const QColor kLightSquare(0xf0, 0xd9, 0xb5);
const QColor kDarkSquare(0xb5, 0x88, 0x63);
const QColor kLastMove(0xcd, 0xd2, 0x6a, 0xb0);
const QColor kSelected(0x64, 0x9f, 0x5a, 0xa0);
const QColor kMoveHint(0x14, 0x33, 0x0f, 0x48);
const QColor kSequenceFrame(0xd4, 0x3f, 0x32);
/// The border of a board showing an explanation, the blue of its reply arrows.
/// Only its colour ever changes: the border stays the same two pixels.
const QColor kExplainFrame(0x3a, 0x6e, 0xb5);
constexpr qreal kFrameWidth = 2.0;
/// One breath of the border while the engine is being waited for.
constexpr int kPulseMs = 1100;
/// Halo around a piece moving on its own, e.g. the engine's answer.
const QColor kSlideHalo(0x2f, 0x8f, 0x44);
constexpr qreal kCornerRadius = 4;
/// Pauses of a played sequence: before the first move and between moves.
constexpr int kSequenceStartMs = 500;
constexpr int kSequenceStepMs = 1100;
constexpr int kSlideMs = 320;
/// However the time is shared, a piece never crosses the board in a blink.
constexpr int kMinimumSlideMs = 140;

/// `from` at t = 0, `to` at t = 1, alpha included.
QColor blend(const QColor &from, const QColor &to, qreal t)
{
    const auto mix = [t](qreal a, qreal b) { return a + (b - a) * t; };
    return QColor::fromRgbF(mix(from.redF(), to.redF()), mix(from.greenF(), to.greenF()),
                            mix(from.blueF(), to.blueF()), mix(from.alphaF(), to.alphaF()));
}

QColor arrowColor(BoardArrow::Kind kind)
{
    switch (kind) {
    case BoardArrow::Kind::Refutation: return QColor(0xd4, 0x3f, 0x32, 0xd0);
    case BoardArrow::Kind::Idea: return QColor(0x2f, 0x8f, 0x44, 0xd0);
    case BoardArrow::Kind::Reply: return QColor(0x3a, 0x6e, 0xb5, 0xc0);
    case BoardArrow::Kind::Alternative: return QColor(0x2f, 0x8f, 0x44, 0xa8);
    }
    return {};
}

} // namespace

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
    , m_board(BoardState::startingPosition())
    , m_sequenceTimer(new QTimer(this))
    , m_slide(new QVariantAnimation(this))
    , m_pulse(new QVariantAnimation(this))
{
    // A plain 0 → 1 loop; the paint turns it into a breath, so the border does
    // not snap back when the animation starts over.
    m_pulse->setStartValue(0.0);
    m_pulse->setEndValue(1.0);
    m_pulse->setDuration(kPulseMs);
    m_pulse->setLoopCount(-1);
    connect(m_pulse, &QVariantAnimation::valueChanged, this, [this] { update(); });

    m_sequenceTimer->setSingleShot(true);
    connect(m_sequenceTimer, &QTimer::timeout, this, &BoardWidget::showNextFrame);
    m_slide->setStartValue(0.0);
    m_slide->setEndValue(1.0);
    m_slide->setDuration(kSlideMs);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slide, &QVariantAnimation::valueChanged, this, [this] { update(); });
    connect(m_slide, &QVariantAnimation::finished, this, [this] {
        // Castling lands the king, then sends the rook.
        if (m_slideStep + 1 < m_slideSteps.size()) {
            ++m_slideStep;
            startSlideStep();
            return;
        }
        m_slideSteps.clear();
        m_slideCaptures.clear();
        update();
    });

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Chessboard"));
}

void BoardWidget::setBorder(BoardBorder border)
{
    if (m_border == border)
        return;
    m_border = border;
    if (border == BoardBorder::Thinking)
        m_pulse->start();
    else
        m_pulse->stop();
    update();
}

void BoardWidget::setBoard(const BoardFrame &frame)
{
    endSequence();
    m_slide->stop();
    m_slideEmphasis = false;
    m_slideCaptures.clear();
    m_slideSteps.clear();
    m_slide->setDuration(kSlideMs);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    m_board = frame.board;
    m_lastMoveFrom = frame.lastMoveFrom;
    m_lastMoveTo = frame.lastMoveTo;
    m_markedKing = frame.markedKing;
    m_kingMark = frame.kingMark;
    m_arrows.clear();
    m_lostPieces.clear();
    clearSelection();
    update();
}

void BoardWidget::setBoardAnimated(const BoardFrame &frame, int durationMs)
{
    const BoardState before = m_board;
    setBoard(frame);
    // Nothing to slide when the piece was not on the board to begin with.
    if (frame.lastMoveFrom < 0 || frame.lastMoveTo < 0 || before.at(frame.lastMoveFrom).isNull())
        return;
    m_slideEmphasis = true;
    m_slide->setEasingCurve(QEasingCurve::InOutCubic);
    startSlide(before, qMax(1, durationMs));
}

void BoardWidget::startSlide(const BoardState &before, int durationMs)
{
    m_slideCaptures.clear();
    m_slideSteps.clear();
    m_slideStep = 0;
    m_slideDurationMs = durationMs;

    const Piece mover = before.at(m_lastMoveFrom);
    m_slideSteps.append({m_lastMoveFrom, m_lastMoveTo, mover});

    // Squares the mover's own side left, and the ones it took, apart from the
    // move itself: castling is the only move that fills them.
    QList<int> vacated;
    QList<int> filled;
    for (int square = 0; square < 64; ++square) {
        const Piece was = before.at(square);
        const Piece now = m_board.at(square);
        if (was == now)
            continue;
        // Only the other side's pieces are captured: a rook that castles moved.
        if (!was.isNull() && was.side != mover.side)
            m_slideCaptures.append({square, was});
        if (!was.isNull() && was.side == mover.side && square != m_lastMoveFrom)
            vacated << square;
        if (!now.isNull() && now.side == mover.side && square != m_lastMoveTo)
            filled << square;
    }
    for (int from : std::as_const(vacated)) {
        const Piece piece = before.at(from);
        for (qsizetype i = 0; i < filled.size(); ++i) {
            if (m_board.at(filled.at(i)) != piece)
                continue;
            m_slideSteps.append({from, filled.takeAt(i), piece});
            break;
        }
    }

    startSlideStep();
}

void BoardWidget::startSlideStep()
{
    // The first piece gets most of the time; the rook of a castling follows in
    // what is left, so the whole move still takes as long as one move should.
    const qreal share = m_slideSteps.size() < 2 ? 1.0 : (m_slideStep == 0 ? 0.6 : 0.4 / qreal(m_slideSteps.size() - 1));
    m_slide->setDuration(qMax(kMinimumSlideMs, int(m_slideDurationMs * share)));
    m_slide->start();
}

void BoardWidget::setLegalMoves(const QMultiHash<int, int> &moves)
{
    m_legalMoves = moves;
    if (m_selected >= 0 && !m_legalMoves.contains(m_selected))
        clearSelection();
    update();
}

void BoardWidget::setExplanation(const QList<BoardArrow> &arrows, const QList<int> &lostPieces)
{
    if (m_arrows == arrows && m_lostPieces == lostPieces)
        return;
    m_arrows = arrows;
    m_lostPieces = lostPieces;
    update();
}

void BoardWidget::playSequence(const QList<BoardFrame> &frames)
{
    if (!m_sequenceActive)
        m_beforeSequence = {m_board, m_lastMoveFrom, m_lastMoveTo, m_markedKing, m_kingMark};
    clearSelection();
    m_frames = frames;
    m_nextFrame = 0;
    m_sequenceActive = true;
    m_sequenceTimer->start(kSequenceStartMs);
    update();
}

void BoardWidget::stopSequence()
{
    if (!m_sequenceActive)
        return;
    const BoardFrame original = m_beforeSequence;
    endSequence();
    m_board = original.board;
    m_lastMoveFrom = original.lastMoveFrom;
    m_lastMoveTo = original.lastMoveTo;
    m_markedKing = original.markedKing;
    m_kingMark = original.kingMark;
    update();
}

void BoardWidget::endSequence()
{
    m_sequenceTimer->stop();
    m_slide->stop();
    m_slideSteps.clear();
    m_slideCaptures.clear();
    m_frames.clear();
    m_sequenceActive = false;
}

void BoardWidget::showNextFrame()
{
    if (!m_sequenceActive || m_nextFrame >= m_frames.size())
        return;
    const BoardFrame &frame = m_frames.at(m_nextFrame++);
    const BoardState before = m_board;
    m_board = frame.board;
    m_lastMoveFrom = frame.lastMoveFrom;
    m_lastMoveTo = frame.lastMoveTo;
    m_markedKing = frame.markedKing;
    m_kingMark = frame.kingMark;
    m_slide->stop();
    if (m_lastMoveFrom >= 0 && m_lastMoveTo >= 0)
        startSlide(before, kSlideMs);
    if (m_nextFrame < m_frames.size())
        m_sequenceTimer->start(kSequenceStepMs);
    update();
}

void BoardWidget::clearSelection()
{
    m_selected = -1;
    m_pressed = false;
    m_dragging = false;
    m_deselectOnRelease = false;
    update();
}

void BoardWidget::setFlipped(bool flipped)
{
    if (m_flipped == flipped)
        return;
    m_flipped = flipped;
    update();
}

void BoardWidget::setShowCoordinates(bool show)
{
    if (m_showCoordinates == show)
        return;
    m_showCoordinates = show;
    update();
}

QSize BoardWidget::sizeHint() const
{
    return {560, 560};
}

QSize BoardWidget::minimumSizeHint() const
{
    return {200, 200};
}

int BoardWidget::sideForAvailable(int available)
{
    // Squares are whole pixels so that edges stay crisp.
    const int squares = qMax(8, ((available - 2 * kMargin) / 8) * 8);
    return squares + 2 * kMargin;
}

QRectF BoardWidget::boardRect() const
{
    const qreal side = std::floor((qMin(width(), height()) - 2.0 * kMargin) / 8) * 8;
    return {(width() - side) / 2, (height() - side) / 2, side, side};
}

QRectF BoardWidget::squareRect(int square) const
{
    const QRectF board = boardRect();
    const qreal size = board.width() / 8;
    int file = square % 8;
    int rank = square / 8;
    if (m_flipped)
        file = 7 - file;
    else
        rank = 7 - rank;
    return {board.left() + file * size, board.top() + rank * size, size, size};
}

int BoardWidget::squareAt(const QPointF &point) const
{
    const QRectF board = boardRect();
    if (!board.contains(point))
        return -1;
    const qreal size = board.width() / 8;
    int file = qBound(0, int((point.x() - board.left()) / size), 7);
    int rank = 7 - qBound(0, int((point.y() - board.top()) / size), 7);
    if (m_flipped) {
        file = 7 - file;
        rank = 7 - rank;
    }
    return rank * 8 + file;
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF board = boardRect();
    const qreal size = board.width() / 8;

    QPainterPath rounded;
    rounded.addRoundedRect(board, kCornerRadius, kCornerRadius);
    painter.save();
    painter.setClipPath(rounded);
    for (int square = 0; square < 64; ++square) {
        const QRectF rect = squareRect(square);
        const bool light = (square / 8 + square % 8) % 2 == 1;
        painter.fillRect(rect, light ? kLightSquare : kDarkSquare);
        if (square == m_lastMoveFrom || square == m_lastMoveTo)
            painter.fillRect(rect, kLastMove);
    }

    if (m_showCoordinates) {
        QFont font = this->font();
        font.setPixelSize(qMax(8, int(size * 0.16)));
        font.setBold(true);
        painter.setFont(font);
        const qreal pad = size * 0.05;
        for (int i = 0; i < 8; ++i) {
            // Files along the bottom row, ranks along the left column as seen on screen.
            const int bottomSquare = m_flipped ? 63 - i : i;
            const int leftSquare = m_flipped ? 63 - i * 8 : i * 8;
            const bool bottomLight = (bottomSquare / 8 + bottomSquare % 8) % 2 == 1;
            const bool leftLight = (leftSquare / 8 + leftSquare % 8) % 2 == 1;

            painter.setPen(bottomLight ? kDarkSquare : kLightSquare);
            painter.drawText(squareRect(bottomSquare).adjusted(pad, pad, -pad, -pad),
                             Qt::AlignRight | Qt::AlignBottom,
                             QString(QChar('a' + bottomSquare % 8)));
            painter.setPen(leftLight ? kDarkSquare : kLightSquare);
            painter.drawText(squareRect(leftSquare).adjusted(pad, pad, -pad, -pad),
                             Qt::AlignLeft | Qt::AlignTop,
                             QString(QChar('1' + leftSquare / 8)));
        }
    }

    if (m_selected >= 0)
        painter.fillRect(squareRect(m_selected), kSelected);
    painter.restore();

    // A neutral frame; red while a sequence (e.g. a mate) is being shown, and
    // blue for an explanation, breathing while the engine is still looking.
    QColor plain = palette().color(QPalette::WindowText);
    plain.setAlphaF(0.28);
    QColor frameColor = plain;
    if (m_sequenceActive) {
        frameColor = kSequenceFrame;
    } else if (m_border == BoardBorder::Explained) {
        frameColor = kExplainFrame;
    } else if (m_border == BoardBorder::Thinking) {
        // cos() turns the looping 0 → 1 into a breath with no seam.
        const qreal breath = 0.5 - 0.5 * std::cos(2 * M_PI * m_pulse->currentValue().toReal());
        frameColor = blend(plain, kExplainFrame, breath);
    }
    painter.setPen(QPen(frameColor, kFrameWidth));
    painter.setBrush(Qt::NoBrush);
    const qreal outset = kFrameWidth / 2;
    painter.drawRoundedRect(board.adjusted(-outset, -outset, outset, outset), kCornerRadius + outset,
                            kCornerRadius + outset);

    const bool sliding = m_slide->state() == QAbstractAnimation::Running && !m_slideSteps.isEmpty();
    // Pieces that have yet to travel are drawn where they still are, so the
    // squares they are heading for stay empty until they get there.
    QSet<int> arriving;
    if (sliding) {
        for (qsizetype i = m_slideStep; i < m_slideSteps.size(); ++i)
            arriving.insert(m_slideSteps.at(i).to);
    }
    // A king in check or mated is marked once the move has landed.
    const bool markKing = m_markedKing >= 0 && m_kingMark != KingMark::None && !sliding;
    if (markKing)
        paintKingGlow(painter);
    for (int square = 0; square < 64; ++square) {
        const Piece piece = m_board.at(square);
        if (piece.isNull() || (m_dragging && square == m_selected) || arriving.contains(square))
            continue;
        paintPiece(painter, piece, squareRect(square));
    }
    if (sliding) {
        // A captured piece stands its ground until the attacker reaches it.
        for (const auto &[square, piece] : m_slideCaptures)
            paintPiece(painter, piece, squareRect(square));
        // The rook of a castling waits on its corner for its turn.
        for (qsizetype i = m_slideStep + 1; i < m_slideSteps.size(); ++i)
            paintPiece(painter, m_slideSteps.at(i).piece, squareRect(m_slideSteps.at(i).from));

        const SlideStep &step = m_slideSteps.at(m_slideStep);
        const qreal progress = m_slide->currentValue().toReal();
        const QRectF from = squareRect(step.from);
        const QRectF to = squareRect(step.to);
        QRectF travelling = from.translated((to.topLeft() - from.topLeft()) * progress);
        if (m_slideEmphasis) {
            // The piece swells and carries a halo that fades in and out, so
            // the eye follows a move nobody asked for.
            const qreal lift = std::sin(progress * M_PI);
            const qreal grow = travelling.width() * 0.22 * lift;
            travelling = travelling.adjusted(-grow, -grow, grow, grow);
            QColor halo = kSlideHalo;
            halo.setAlphaF(0.25 + 0.5 * lift);
            painter.setPen(QPen(halo, qMax(2.0, size * 0.09)));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(travelling.center(), travelling.width() * 0.52, travelling.width() * 0.52);
        }
        paintPiece(painter, step.piece, travelling);
    }

    // Targets of the selected piece: dots on empty squares, rings on captures.
    if (m_selected >= 0) {
        painter.setPen(Qt::NoPen);
        for (int target : m_legalMoves.values(m_selected)) {
            const QRectF rect = squareRect(target);
            if (m_board.at(target).isNull()) {
                painter.setBrush(kMoveHint);
                painter.drawEllipse(rect.center(), size * 0.16, size * 0.16);
            } else {
                QPainterPath ring;
                ring.addRect(rect);
                ring.addEllipse(rect.center(), size * 0.56, size * 0.56);
                painter.fillPath(ring, kMoveHint);
            }
        }
    }

    // Arrows belong to the position the sequence started from.
    for (int square : m_sequenceActive ? QList<int>() : m_lostPieces) {
        painter.setPen(QPen(arrowColor(BoardArrow::Kind::Refutation), qMax(2.0, size * 0.06)));
        painter.setBrush(Qt::NoBrush);
        const qreal inset = size * 0.07;
        painter.drawEllipse(squareRect(square).adjusted(inset, inset, -inset, -inset));
    }
    for (const BoardArrow &arrow : m_sequenceActive ? QList<BoardArrow>() : m_arrows)
        paintArrow(painter, arrow);

    if (markKing)
        paintKingBadge(painter);

    if (m_dragging && m_selected >= 0) {
        QRectF rect(0, 0, size, size);
        rect.moveCenter(m_dragPosition);
        paintPiece(painter, m_board.at(m_selected), rect);
    }

    // Like :focus-visible, show the ring only when focus came from the keyboard.
    if (hasFocus() && m_keyboardFocus) {
        painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(board.adjusted(-4, -4, 4, 4), kCornerRadius + 4, kCornerRadius + 4);
    }
}

void BoardWidget::paintPiece(QPainter &painter, Piece piece, const QRectF &rect) const
{
    PieceRenderer::paint(painter, piece, rect, devicePixelRatioF());
}

void BoardWidget::paintKingGlow(QPainter &painter) const
{
    const QRectF rect = squareRect(m_markedKing);
    const qreal size = rect.width();
    // Lighter for a check than for a mate.
    const int alpha = m_kingMark == KingMark::Mate ? 0xc0 : 0x80;
    QRadialGradient glow(rect.center(), size * 0.55);
    glow.setColorAt(0.0, QColor(0xd4, 0x3f, 0x32, alpha));
    glow.setColorAt(0.55, QColor(0xd4, 0x3f, 0x32, alpha * 3 / 5));
    glow.setColorAt(1.0, QColor(0xd4, 0x3f, 0x32, 0x00));
    painter.save();
    painter.setClipRect(rect);
    painter.fillRect(rect, glow);
    painter.restore();
}

void BoardWidget::paintKingBadge(QPainter &painter) const
{
    // The chess sign in the corner of the king's square: "+" check, "#" mate.
    const QRectF rect = squareRect(m_markedKing);
    const qreal size = rect.width();
    const qreal radius = size * 0.14;
    const QPointF badge(rect.right() - radius - size * 0.04, rect.top() + radius + size * 0.04);
    QColor fill = kSequenceFrame;
    if (m_kingMark == KingMark::Check)
        fill.setAlpha(0xd8);
    painter.setPen(QPen(QColor(255, 255, 255, 220), qMax(1.0, size * 0.02)));
    painter.setBrush(fill);
    painter.drawEllipse(badge, radius, radius);
    QFont font = this->font();
    font.setPixelSize(qMax(8, int(radius * 1.3)));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(badge.x() - radius, badge.y() - radius, 2 * radius, 2 * radius), Qt::AlignCenter,
                     m_kingMark == KingMark::Mate ? QStringLiteral("#") : QStringLiteral("+"));
}

void BoardWidget::paintArrow(QPainter &painter, const BoardArrow &arrow) const
{
    if (arrow.from < 0 || arrow.to < 0 || arrow.from == arrow.to)
        return;
    const qreal size = boardRect().width() / 8;
    const QColor color = arrowColor(arrow.kind);

    // Knight moves bend like the knight goes: along the long leg first.
    QList<QPointF> points{squareRect(arrow.from).center()};
    const int fileDistance = qAbs(arrow.to % 8 - arrow.from % 8);
    const int rankDistance = qAbs(arrow.to / 8 - arrow.from / 8);
    if (fileDistance + rankDistance == 3 && fileDistance > 0 && rankDistance > 0) {
        const int corner = rankDistance == 2 ? (arrow.to / 8) * 8 + arrow.from % 8
                                             : (arrow.from / 8) * 8 + arrow.to % 8;
        points << squareRect(corner).center();
    }
    points << squareRect(arrow.to).center();

    const auto unitVector = [](const QPointF &from, const QPointF &to) {
        const QPointF delta = to - from;
        const qreal length = std::hypot(delta.x(), delta.y());
        return length > 0 ? delta / length : QPointF();
    };
    const QPointF lastFrom = points.at(points.size() - 2);
    const QPointF lastUnit = unitVector(lastFrom, points.last());
    const qreal lastLength = std::hypot(points.last().x() - lastFrom.x(), points.last().y() - lastFrom.y());
    const QPointF tip = points.last() - lastUnit * size * 0.1;
    // Arrows to a neighbouring square get a shorter head, leaving room for the step number.
    const QPointF headBase = tip - lastUnit * qMin(size * 0.42, lastLength * 0.3);
    const QPointF start = points.first() + unitVector(points.at(0), points.at(1)) * size * 0.2;

    QPainterPath shaft(start);
    for (qsizetype i = 1; i + 1 < points.size(); ++i)
        shaft.lineTo(points.at(i));
    shaft.lineTo(headBase);
    QPen pen(color, size * 0.15, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
    if (arrow.kind == BoardArrow::Kind::Alternative)
        pen.setDashPattern({0.9, 0.6});
    painter.strokePath(shaft, pen);

    const QPointF normal(-lastUnit.y(), lastUnit.x());
    const qreal halfHead = size * 0.22;
    QPainterPath head(tip);
    head.lineTo(headBase + normal * halfHead);
    head.lineTo(headBase - normal * halfHead);
    head.closeSubpath();
    painter.fillPath(head, color);

    if (arrow.step <= 0)
        return;
    // Step number where the arrow leaves its square, so that it stays readable
    // when several arrows end on the same square.
    const qreal radius = size * 0.15;
    const QPointF badge = points.first() + unitVector(points.at(0), points.at(1)) * size * 0.4;
    QColor badgeColor = color;
    badgeColor.setAlpha(255);
    painter.setPen(QPen(QColor(255, 255, 255, 220), qMax(1.0, size * 0.02)));
    painter.setBrush(badgeColor.darker(115));
    painter.drawEllipse(badge, radius, radius);
    QFont font = this->font();
    font.setPixelSize(qMax(8, int(radius * 1.3)));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(badge.x() - radius, badge.y() - radius, 2 * radius, 2 * radius), Qt::AlignCenter,
                     QString::number(arrow.step));
}

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_sequenceActive) {
        QWidget::mousePressEvent(event); // The shown position is not the one to play from.
        return;
    }
    if (event->button() != Qt::LeftButton) {
        clearSelection();
        QWidget::mousePressEvent(event);
        return;
    }
    const int square = squareAt(event->position());
    if (m_selected >= 0 && square >= 0 && m_legalMoves.contains(m_selected, square)) {
        const int from = m_selected;
        clearSelection();
        Q_EMIT moveRequested(from, square, event->globalPosition().toPoint());
        return;
    }
    if (square >= 0 && m_legalMoves.contains(square)) {
        m_deselectOnRelease = square == m_selected;
        m_selected = square;
        m_pressed = true;
        m_pressPosition = event->position();
        update();
        return;
    }
    clearSelection();
}

void BoardWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed)
        return;
    if (!m_dragging && (event->position() - m_pressPosition).manhattanLength() < QApplication::startDragDistance())
        return;
    m_dragging = true;
    m_dragPosition = event->position();
    update();
}

void BoardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    if (!m_dragging) {
        if (m_deselectOnRelease)
            clearSelection();
        return;
    }

    m_dragging = false;
    const int from = m_selected;
    const int square = squareAt(event->position());
    if (square == from) {
        update(); // Dropped back: keep the piece selected for a click on the target.
        return;
    }
    clearSelection();
    if (square >= 0 && m_legalMoves.contains(from, square))
        Q_EMIT moveRequested(from, square, event->globalPosition().toPoint());
}

void BoardWidget::wheelEvent(QWheelEvent *event)
{
    m_wheelAccumulator += event->angleDelta().y();
    const int steps = m_wheelAccumulator / 120;
    if (steps != 0) {
        m_wheelAccumulator -= steps * 120;
        // Scrolling down moves forward through the game.
        Q_EMIT navigateRequested(-steps);
    }
    event->accept();
}

void BoardWidget::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QWidget::focusInEvent(event);
}

void BoardWidget::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QWidget::focusOutEvent(event);
}
