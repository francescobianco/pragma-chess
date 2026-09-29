#pragma once

#include "app/BoardSettings.h"

#include <QDialog>

class QCheckBox;
class QRadioButton;

/// Options ▸ Board Settings…: how the board and what surrounds it are drawn.
class BoardSettingsDialog : public QDialog {
    Q_OBJECT

public:
    BoardSettingsDialog(const BoardSettings &settings, bool showCoordinates, QWidget *parent = nullptr);

    BoardSettings settings() const;
    /// Coordinates belong to the project (View ▸ Show Coordinates), so they are returned apart.
    bool showCoordinates() const;

private:
    QRadioButton *m_besideBoard;
    QRadioButton *m_belowBoard;
    QCheckBox *m_showTurn;
    QCheckBox *m_showCoordinates;
};
