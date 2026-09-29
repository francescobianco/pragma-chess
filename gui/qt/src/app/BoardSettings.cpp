#include "BoardSettings.h"

#include <QSettings>

namespace {

const QString kCapturedPiecesKey = QStringLiteral("board/capturedPieces");
const QString kShowTurnKey = QStringLiteral("board/showTurn");
const QString kBelow = QStringLiteral("below");
const QString kBeside = QStringLiteral("beside");

} // namespace

BoardSettings BoardSettings::load()
{
    QSettings settings;
    BoardSettings board;
    if (settings.value(kCapturedPiecesKey).toString() == kBelow)
        board.capturedPieces = CapturedPiecesPlacement::BelowBoard;
    board.showTurn = settings.value(kShowTurnKey, true).toBool();
    return board;
}

void BoardSettings::save() const
{
    QSettings settings;
    settings.setValue(kCapturedPiecesKey, capturedPieces == CapturedPiecesPlacement::BelowBoard ? kBelow : kBeside);
    settings.setValue(kShowTurnKey, showTurn);
}
