#pragma once

#include "app/GraphicsSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QRadioButton;

/// Options ▸ Graphics Settings…: light or dark, and how the board and what
/// surrounds it are drawn. Kept on this computer.
class GraphicsSettingsDialog : public QDialog {
    Q_OBJECT

public:
    GraphicsSettingsDialog(const GraphicsSettings &settings, bool showCoordinates, QWidget *parent = nullptr);

    GraphicsSettings settings() const;
    /// Coordinates belong to the project (View ▸ Show Coordinates), so they are returned apart.
    bool showCoordinates() const;

private:
    QComboBox *m_appearance;
    QRadioButton *m_besideBoard;
    QRadioButton *m_belowBoard;
    QCheckBox *m_showTurn;
    QCheckBox *m_showCoordinates;
    QCheckBox *m_moveSound;
};
