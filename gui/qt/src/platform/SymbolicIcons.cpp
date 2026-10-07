#include "SymbolicIcons.h"

#include <QtMath>
#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

namespace {

enum class Shape {
    NewDocument,
    Open,
    Save,
    SaveAs,
    Folder,
    First,
    Previous,
    Next,
    Last,
    Flip,
    Play,
    Stop,
    Search,
    Copy,
    Paste,
    Quit,
    About,
    Explain,
    NewGame,
    Database,
    Sync,
    Training,
    Online,
    Book,
    Engine,
    Eye,
    SendMove,
    SendPlan,
};

/// The square New Game and New Training share: a board, and a face as large.
const QRectF kBoardIcon(2.5, 2.5, 11, 11);

/// Paints a shape on a 16×16 grid with the given color.
void paintShape(QPainter *painter, Shape shape, const QColor &color)
{
    QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    const auto chevron = [&](qreal tipX, qreal backX) {
        QPainterPath path;
        path.moveTo(backX, 3.5);
        path.lineTo(tipX, 8);
        path.lineTo(backX, 12.5);
        painter->drawPath(path);
    };
    const auto page = [&](qreal left, qreal top, qreal right, qreal bottom) {
        QPainterPath path;
        path.moveTo(left, top);
        path.lineTo(right - 3.5, top);
        path.lineTo(right, top + 3.5);
        path.lineTo(right, bottom);
        path.lineTo(left, bottom);
        path.closeSubpath();
        path.moveTo(right - 3.5, top);
        path.lineTo(right - 3.5, top + 3.5);
        path.lineTo(right, top + 3.5);
        painter->drawPath(path);
    };

    switch (shape) {
    case Shape::NewDocument:
        page(3, 1.75, 13, 14.25);
        painter->drawLine(QPointF(8, 7), QPointF(8, 11.5));
        painter->drawLine(QPointF(5.75, 9.25), QPointF(10.25, 9.25));
        break;
    case Shape::Open:
    case Shape::Folder: {
        QPainterPath path;
        path.moveTo(1.75, 3.25);
        path.lineTo(6, 3.25);
        path.lineTo(7.5, 4.75);
        path.lineTo(14.25, 4.75);
        path.lineTo(14.25, 12.75);
        path.lineTo(1.75, 12.75);
        path.closeSubpath();
        painter->drawPath(path);
        if (shape == Shape::Open)
            painter->drawLine(QPointF(1.75, 7), QPointF(14.25, 7));
        break;
    }
    case Shape::Save:
    case Shape::SaveAs: {
        // The classic floppy disk: a clipped corner, the shutter above, the
        // label below.
        QPainterPath disk;
        disk.moveTo(2.5, 2.5);
        disk.lineTo(10.75, 2.5);
        disk.lineTo(13.5, 5.25);
        disk.lineTo(13.5, 13.5);
        disk.lineTo(2.5, 13.5);
        disk.closeSubpath();
        painter->drawPath(disk);
        painter->drawRect(QRectF(5, 2.5, 5.5, 3.5));              // The shutter.
        painter->fillRect(QRectF(8.25, 3.25, 1.25, 2), color);     // Its slot.
        painter->drawRect(QRectF(4.5, 9, 7, 4.5));                 // The label.
        if (shape == Shape::SaveAs)
            painter->drawLine(QPointF(6, 11.25), QPointF(10, 11.25));
        break;
    }
    case Shape::First:
        painter->drawLine(QPointF(4, 3.5), QPointF(4, 12.5));
        chevron(6.5, 11);
        break;
    case Shape::Previous:
        chevron(5.5, 10);
        break;
    case Shape::Next:
        chevron(10.5, 6);
        break;
    case Shape::Last:
        painter->drawLine(QPointF(12, 3.5), QPointF(12, 12.5));
        chevron(9.5, 5);
        break;
    case Shape::Flip: {
        // Two opposite arrows: turn the board around.
        painter->drawLine(QPointF(5, 13), QPointF(5, 3));
        painter->drawLine(QPointF(2.5, 5.5), QPointF(5, 3));
        painter->drawLine(QPointF(7.5, 5.5), QPointF(5, 3));
        painter->drawLine(QPointF(11, 3), QPointF(11, 13));
        painter->drawLine(QPointF(8.5, 10.5), QPointF(11, 13));
        painter->drawLine(QPointF(13.5, 10.5), QPointF(11, 13));
        break;
    }
    case Shape::Training: {
        // The opponent of a training game: a square face, as large as the
        // board of New Game, with almond eyes and a narrow mouth.
        painter->drawRoundedRect(kBoardIcon, 1.25, 1.25);
        for (const qreal x : {5.6, 10.4}) {
            QPainterPath eye;
            eye.moveTo(x - 1.5, 6.6);
            eye.quadTo(x, 4.9, x + 1.5, 6.6);
            eye.quadTo(x, 8.3, x - 1.5, 6.6);
            painter->fillPath(eye, color);
        }
        painter->drawLine(QPointF(6.75, 10.6), QPointF(9.25, 10.6));
        break;
    }
    case Shape::Online: {
        // Playing online: the same square as New Game and New Training, with
        // the world inside — a globe with its equator and a meridian.
        painter->drawRoundedRect(kBoardIcon, 1.25, 1.25);
        const QRectF globe(4.75, 4.75, 6.5, 6.5);
        painter->setPen(QPen(color, 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawEllipse(globe);
        painter->drawLine(QPointF(globe.left(), 8), QPointF(globe.right(), 8));
        painter->drawEllipse(QRectF(6.6, globe.top(), 2.8, globe.height()));
        break;
    }
    case Shape::Sync: {
        // Two arrows chasing each other round, clockwise, well apart: each
        // head stops short of the other's tail, so they read as two arrows
        // and not as a circle.
        const QPointF center(8, 8);
        const qreal radius = 5.25;
        const QRectF circle(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius);
        const auto onCircle = [&](qreal degrees) {
            const qreal angle = qDegreesToRadians(degrees);
            return center + radius * QPointF(qCos(angle), -qSin(angle));
        };
        for (const qreal tail : {160.0, 340.0}) {
            // The shaft, then a solid head along the chord from its base to
            // its tip: on so small a circle the tangent would point outwards.
            const qreal base = tail - 90;
            const qreal tip = base - 40;
            painter->drawArc(circle, qRound(base * 16), 90 * 16);
            const QPointF along = onCircle(tip) - onCircle(base);
            const QPointF across = QPointF(-along.y(), along.x()) * (1.9 / qSqrt(QPointF::dotProduct(along, along)));
            QPainterPath head;
            head.moveTo(onCircle(tip));
            head.lineTo(onCircle(base) + across);
            head.lineTo(onCircle(base) - across);
            head.closeSubpath();
            painter->save();
            painter->setPen(QPen(color, 0.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(color);
            painter->drawPath(head);
            painter->restore();
        }
        break;
    }
    case Shape::SendMove: {
        // Send Move: a paper plane, flying up and right, with the fold of its wing.
        QPainterPath plane;
        plane.moveTo(1.75, 7.25);
        plane.lineTo(14.25, 1.75);
        plane.lineTo(9.75, 14.25);
        plane.lineTo(7.25, 8.75);
        plane.closeSubpath();
        painter->drawPath(plane);
        painter->drawLine(QPointF(7.25, 8.75), QPointF(14.25, 1.75));
        break;
    }
    case Shape::SendPlan: {
        // Send Plan: a small decision tree — the move on the left, and the
        // two answers prepared for the opponent's replies, square nodes
        // joined at right angles, like a chart of the plan.
        const QRectF move(1.75, 6, 4, 4);
        const QRectF upper(10.25, 1.75, 4, 4);
        const QRectF lower(10.25, 10.25, 4, 4);
        painter->setBrush(color);
        painter->drawRoundedRect(move, 0.75, 0.75);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(upper, 0.75, 0.75);
        painter->drawRoundedRect(lower, 0.75, 0.75);
        QPainterPath links;
        links.moveTo(move.right(), 8);
        links.lineTo(8, 8);
        links.moveTo(upper.left(), upper.center().y());
        links.lineTo(8, upper.center().y());
        links.lineTo(8, lower.center().y());
        links.lineTo(lower.left(), lower.center().y());
        painter->drawPath(links);
        break;
    }
    case Shape::Play: {
        QPainterPath path;
        path.moveTo(4.5, 2.75);
        path.lineTo(13, 8);
        path.lineTo(4.5, 13.25);
        path.closeSubpath();
        painter->setBrush(color);
        painter->drawPath(path);
        break;
    }
    case Shape::Stop:
        painter->setBrush(color);
        painter->drawRoundedRect(QRectF(3.5, 3.5, 9, 9), 1.5, 1.5);
        break;
    case Shape::Search:
        painter->drawEllipse(QPointF(6.75, 6.75), 4.25, 4.25);
        painter->drawLine(QPointF(10, 10), QPointF(13.75, 13.75));
        break;
    case Shape::Copy:
        painter->drawRoundedRect(QRectF(5.25, 5.25, 8.5, 8.5), 1.25, 1.25);
        painter->drawLine(QPointF(2.25, 10.25), QPointF(2.25, 3.5));
        painter->drawLine(QPointF(3.5, 2.25), QPointF(10.25, 2.25));
        break;
    case Shape::Paste:
        painter->drawRoundedRect(QRectF(3, 3, 10, 11), 1.25, 1.25);
        painter->drawLine(QPointF(6, 2), QPointF(10, 2));
        painter->drawLine(QPointF(5.75, 7.5), QPointF(10.25, 7.5));
        painter->drawLine(QPointF(5.75, 10.5), QPointF(10.25, 10.5));
        break;
    case Shape::Quit:
        painter->drawLine(QPointF(4, 4), QPointF(12, 12));
        painter->drawLine(QPointF(12, 4), QPointF(4, 12));
        break;
    case Shape::Explain: {
        // A light bulb: the idea behind the move.
        QPainterPath bulb;
        bulb.moveTo(6, 10.5);
        bulb.cubicTo(6, 9, 3.25, 7.75, 3.25, 5.75);
        bulb.cubicTo(3.25, 3.1, 5.4, 1.5, 8, 1.5);
        bulb.cubicTo(10.6, 1.5, 12.75, 3.1, 12.75, 5.75);
        bulb.cubicTo(12.75, 7.75, 10, 9, 10, 10.5);
        bulb.closeSubpath();
        painter->drawPath(bulb);
        painter->drawLine(QPointF(6.25, 12.75), QPointF(9.75, 12.75));
        painter->drawLine(QPointF(7, 14.75), QPointF(9, 14.75));
        break;
    }
    case Shape::Eye: {
        // An eye: the end of the engine's line, seen while it is held.
        QPainterPath eye;
        eye.moveTo(1.5, 8);
        eye.quadTo(8, 1, 14.5, 8);
        eye.quadTo(8, 15, 1.5, 8);
        eye.closeSubpath();
        painter->drawPath(eye);
        QPainterPath pupil;
        pupil.addEllipse(QPointF(8, 8), 2.25, 2.25);
        painter->fillPath(pupil, color);
        break;
    }
    case Shape::NewGame: {
        // A board of four squares, two light and two dark (a1 is dark),
        // drawn with the same edge as New Training and New Online Game.
        const QRectF square = kBoardIcon;
        QPainterPath board;
        board.addRoundedRect(square, 1.25, 1.25);
        QPainterPath dark;
        dark.addRect(QRectF(square.center().x(), square.top(), square.width() / 2, square.height() / 2));
        dark.addRect(QRectF(square.left(), square.center().y(), square.width() / 2, square.height() / 2));
        painter->fillPath(board.intersected(dark), color);
        painter->drawPath(board);
        break;
    }
    case Shape::Book: {
        // An open book: two pages meeting at the spine.
        QPainterPath pages;
        pages.moveTo(8, 4.5);
        pages.quadTo(5.25, 2.75, 2.25, 3.5);
        pages.lineTo(2.25, 12);
        pages.quadTo(5.25, 11.25, 8, 13);
        pages.quadTo(10.75, 11.25, 13.75, 12);
        pages.lineTo(13.75, 3.5);
        pages.quadTo(10.75, 2.75, 8, 4.5);
        pages.lineTo(8, 13);
        painter->drawPath(pages);
        break;
    }
    case Shape::Engine: {
        // A gear: what does the calculating. Solid, with its hole.
        const QPointF center(8, 8);
        const auto at = [&](qreal degrees, qreal radius) {
            const qreal angle = qDegreesToRadians(degrees);
            return center + radius * QPointF(qCos(angle), qSin(angle));
        };
        QPainterPath gear;
        for (int tooth = 0; tooth < 8; ++tooth) {
            const qreal middle = 45.0 * tooth;
            const QPointF first = at(middle - 15, 4.6);
            if (tooth == 0)
                gear.moveTo(first);
            else
                gear.lineTo(first);
            gear.lineTo(at(middle - 9, 6.3));
            gear.lineTo(at(middle + 9, 6.3));
            gear.lineTo(at(middle + 15, 4.6));
        }
        gear.closeSubpath();
        gear.addEllipse(center, 2.3, 2.3); // Odd-even fill leaves it empty.
        painter->setPen(QPen(color, 0.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(color);
        painter->drawPath(gear);
        break;
    }
    case Shape::Database: {
        // A cylinder: three stacked discs.
        painter->drawEllipse(QRectF(2.75, 1.75, 10.5, 3.5));
        QPainterPath body;
        body.moveTo(2.75, 3.5);
        body.lineTo(2.75, 12.5);
        body.arcTo(QRectF(2.75, 10.75, 10.5, 3.5), 180, 180);
        body.lineTo(13.25, 3.5);
        painter->drawPath(body);
        QPainterPath middle;
        middle.moveTo(2.75, 8);
        middle.arcTo(QRectF(2.75, 6.25, 10.5, 3.5), 180, 180);
        painter->drawPath(middle);
        break;
    }
    case Shape::About:
        painter->drawEllipse(QPointF(8, 8), 6, 6);
        painter->drawLine(QPointF(8, 7.25), QPointF(8, 11));
        painter->setBrush(color);
        painter->drawEllipse(QPointF(8, 4.9), 0.35, 0.35);
        break;
    }
}

class SymbolicIconEngine : public QIconEngine {
public:
    explicit SymbolicIconEngine(Shape shape)
        : m_shape(shape)
    {
    }

    QIconEngine *clone() const override { return new SymbolicIconEngine(m_shape); }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const QPalette palette = QApplication::palette();
        QColor color = palette.color(QPalette::Normal, QPalette::WindowText);
        if (mode == QIcon::Disabled)
            color = palette.color(QPalette::Disabled, QPalette::WindowText);
        else if (mode == QIcon::Selected)
            color = palette.color(QPalette::Normal, QPalette::HighlightedText);

        const qreal side = qMin(rect.width(), rect.height());
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0);
        painter->scale(side / 16.0, side / 16.0);
        paintShape(painter, m_shape, color);
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pixmap;
    }

    QString key() const override { return QStringLiteral("pragma-symbolic"); }

private:
    Shape m_shape;
};

} // namespace

namespace SymbolicIcons {

QIcon icon(const QString &name)
{
    static const QHash<QString, Shape> shapes{
        {QStringLiteral("document-new"), Shape::NewDocument},
        {QStringLiteral("document-open"), Shape::Open},
        {QStringLiteral("document-save"), Shape::Save},
        {QStringLiteral("document-save-as"), Shape::SaveAs},
        {QStringLiteral("folder"), Shape::Folder},
        {QStringLiteral("folder-open"), Shape::Open},
        {QStringLiteral("go-first"), Shape::First},
        {QStringLiteral("go-previous"), Shape::Previous},
        {QStringLiteral("go-next"), Shape::Next},
        {QStringLiteral("go-last"), Shape::Last},
        {QStringLiteral("object-flip-vertical"), Shape::Flip},
        {QStringLiteral("media-playback-start"), Shape::Play},
        {QStringLiteral("media-playback-stop"), Shape::Stop},
        {QStringLiteral("edit-find"), Shape::Search},
        {QStringLiteral("edit-copy"), Shape::Copy},
        {QStringLiteral("edit-paste"), Shape::Paste},
        {QStringLiteral("application-exit"), Shape::Quit},
        {QStringLiteral("help-about"), Shape::About},
        {QStringLiteral("pragma-explain"), Shape::Explain},
        {QStringLiteral("pragma-new-game"), Shape::NewGame},
        {QStringLiteral("pragma-database"), Shape::Database},
        {QStringLiteral("view-refresh"), Shape::Sync},
        {QStringLiteral("pragma-training"), Shape::Training},
        {QStringLiteral("pragma-online"), Shape::Online},
        {QStringLiteral("pragma-book"), Shape::Book},
        {QStringLiteral("pragma-engine"), Shape::Engine},
        {QStringLiteral("pragma-eye"), Shape::Eye},
        {QStringLiteral("pragma-send-move"), Shape::SendMove},
        {QStringLiteral("pragma-send-plan"), Shape::SendPlan},
    };
    const auto it = shapes.constFind(name);
    if (it == shapes.cend())
        return {};
    return QIcon(new SymbolicIconEngine(*it));
}

} // namespace SymbolicIcons
