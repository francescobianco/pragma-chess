#pragma once

/// Where the pieces captured so far are shown.
enum class CapturedPiecesPlacement {
    /// In the column at the right of the board, from each player's pawn rank inwards.
    BesideBoard,
    /// In a row under the board, at the left of the game controls.
    BelowBoard,
};

/// How the board is drawn (Options ▸ Board Settings…). A per-user preference,
/// like the language: not part of a project.
struct BoardSettings {
    CapturedPiecesPlacement capturedPieces = CapturedPiecesPlacement::BesideBoard;
    /// The dot on the side of the player to move.
    bool showTurn = true;

    static BoardSettings load();
    void save() const;

    bool operator==(const BoardSettings &) const = default;
};
