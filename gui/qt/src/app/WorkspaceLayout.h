#pragma once

/// The arrangement of the panels of the main window, as a project stores
/// it: which panels are shown and how the space is shared, in per cent of
/// the area the panels can occupy (not the title bar, menu, toolbar or
/// status bar), so a project opened on another screen looks the same.
/// Shares are kept for hidden panels too: showing one again gives it the
/// room it had.
struct WorkspaceLayout {
    bool toolbar = true;
    bool moves = true;
    bool openingTree = false;
    bool engine = true;
    bool games = true;

    /// Height of the Games panel (tree and list), of the usable height.
    int gamesHeight = 32;
    /// Width of the Moves panel in its row with the Opening Tree.
    int movesWidth = 50;
    /// Height of the Engine panel, of the height right of the board.
    int engineHeight = 25;
    /// Width of the database tree in the Games panel.
    int treeWidth = 22;

    /// A share as the file may carry it: clamped so that every panel keeps
    /// some room.
    static int clamped(int percent) { return percent < 5 ? 5 : percent > 95 ? 95 : percent; }

    bool operator==(const WorkspaceLayout &) const = default;
};
