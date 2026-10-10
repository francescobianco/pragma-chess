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
    bool openingTree = true;
    bool engine = true;
    bool games = true;

    /// Height of the Games panel (tree and list), of the usable height.
    double gamesHeight = 32;
    /// Width of the Moves panel in its row with the Opening Tree.
    double movesWidth = 33.33;
    /// Height of the Engine panel, of the height right of the board.
    double engineHeight = 25;
    /// Width of the database tree in the Games panel.
    double treeWidth = 22;

    /// A share as the file may carry it: two decimals (whole per cents
    /// quantize a tall window visibly), clamped so that every panel keeps
    /// some room.
    static double clamped(double percent)
    {
        const double rounded = static_cast<double>(static_cast<long long>(percent * 100 + (percent < 0 ? -0.5 : 0.5))) / 100;
        return rounded < 5 ? 5 : rounded > 95 ? 95 : rounded;
    }
    /// The pixels a share of `whole` comes to.
    static int pixels(double percent, int whole) { return static_cast<int>(whole * percent / 100 + 0.5); }

    bool operator==(const WorkspaceLayout &) const = default;
};
