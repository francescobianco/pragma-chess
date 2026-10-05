#include "GraphicsSettings.h"

#include <QSettings>

namespace {

const QString kCapturedPiecesKey = QStringLiteral("board/capturedPieces");
const QString kShowTurnKey = QStringLiteral("board/showTurn");
const QString kMoveSoundKey = QStringLiteral("sound/moves");
const QString kAppearanceKey = QStringLiteral("appearance/mode");
const QString kBelow = QStringLiteral("below");
const QString kBeside = QStringLiteral("beside");

} // namespace

GraphicsSettings GraphicsSettings::load()
{
    QSettings settings;
    GraphicsSettings board;
    if (settings.value(kCapturedPiecesKey).toString() == kBelow)
        board.capturedPieces = CapturedPiecesPlacement::BelowBoard;
    board.showTurn = settings.value(kShowTurnKey, true).toBool();
    board.moveSound = settings.value(kMoveSoundKey, true).toBool();
    const QString appearance = settings.value(kAppearanceKey).toString();
    board.appearance = appearance == QLatin1String("light")  ? AppearanceMode::Light
                       : appearance == QLatin1String("dark") ? AppearanceMode::Dark
                                                             : AppearanceMode::System;
    return board;
}

void GraphicsSettings::save() const
{
    QSettings settings;
    settings.setValue(kCapturedPiecesKey, capturedPieces == CapturedPiecesPlacement::BelowBoard ? kBelow : kBeside);
    settings.setValue(kShowTurnKey, showTurn);
    settings.setValue(kMoveSoundKey, moveSound);
    settings.setValue(kAppearanceKey, appearance == AppearanceMode::Light  ? QStringLiteral("light")
                                      : appearance == AppearanceMode::Dark ? QStringLiteral("dark")
                                                                           : QStringLiteral("system"));
}
