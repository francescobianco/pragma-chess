#include "SymbolicIcons.h"

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
};

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
        painter->drawRoundedRect(QRectF(2.25, 2.25, 11.5, 11.5), 1.5, 1.5);
        painter->drawLine(QPointF(8, 4.5), QPointF(8, 9.5));
        QPainterPath arrow;
        arrow.moveTo(5.75, 7.5);
        arrow.lineTo(8, 9.75);
        arrow.lineTo(10.25, 7.5);
        painter->drawPath(arrow);
        if (shape == Shape::SaveAs)
            painter->drawLine(QPointF(5, 12), QPointF(11, 12));
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
    case Shape::NewGame: {
        // A pawn with a plus.
        painter->drawEllipse(QPointF(6.5, 4.25), 2.25, 2.25);
        QPainterPath body;
        body.moveTo(4.75, 7.5);
        body.lineTo(8.25, 7.5);
        body.lineTo(9.5, 12);
        body.lineTo(3.5, 12);
        body.closeSubpath();
        painter->drawPath(body);
        painter->drawLine(QPointF(2.25, 14.25), QPointF(10.75, 14.25));
        painter->drawLine(QPointF(13, 2), QPointF(13, 7));
        painter->drawLine(QPointF(10.5, 4.5), QPointF(15.5, 4.5));
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
    };
    const auto it = shapes.constFind(name);
    if (it == shapes.cend())
        return {};
    return QIcon(new SymbolicIconEngine(*it));
}

} // namespace SymbolicIcons
