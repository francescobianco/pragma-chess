#pragma once

/// Where the pieces captured so far are shown.
enum class CapturedPiecesPlacement {
    /// In the column at the right of the board, from each player's pawn rank inwards.
    BesideBoard,
    /// In a row under the board, at the left of the game controls.
    BelowBoard,
};

/// Light or dark.
enum class AppearanceMode {
    /// Whatever the system is.
    System,
    Light,
    Dark,
};

/// How the application and the board are drawn (Options ▸ Graphics
/// Settings…). Per computer, like the language: not part of a project, and
/// not synced — light or dark belongs with the desktop around it.
struct GraphicsSettings {
    AppearanceMode appearance = AppearanceMode::System;
    CapturedPiecesPlacement capturedPieces = CapturedPiecesPlacement::BesideBoard;
    /// The dot on the side of the player to move.
    bool showTurn = true;

    static GraphicsSettings load();
    void save() const;

    bool operator==(const GraphicsSettings &) const = default;
};
