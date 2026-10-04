#pragma once

#include "app/PositionSetup.h"

#include <QDialog>

class PositionEditorWidget;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;

/// Game ▸ Set Up Position…: a board to draw a position on, with who moves,
/// castling, en passant and the move number; OK only for a position a game
/// can start from.
class PositionSetupDialog : public QDialog {
    Q_OBJECT

public:
    PositionSetupDialog(const QString &fen, bool flipped, QWidget *parent = nullptr);

    /// The position set up, as a FEN.
    QString fen() const;

private:
    /// Shows the setup in the controls (and the FEN, unless it is being typed).
    void showSetup(bool fenToo = true);
    /// Reads the controls into the setup.
    void readControls();

    PositionEditorWidget *m_board;
    QButtonGroup *m_palette;
    QRadioButton *m_whiteToMove;
    QRadioButton *m_blackToMove;
    QCheckBox *m_whiteKingSide;
    QCheckBox *m_whiteQueenSide;
    QCheckBox *m_blackKingSide;
    QCheckBox *m_blackQueenSide;
    QComboBox *m_enPassant;
    QSpinBox *m_moveNumber;
    QLineEdit *m_fen;
    QLabel *m_problem;
    QDialogButtonBox *m_buttons;
    bool m_updating = false;
};
