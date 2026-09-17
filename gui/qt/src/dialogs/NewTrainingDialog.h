#pragma once

#include "app/BoardState.h"

#include <QDialog>

class QRadioButton;

/// Asks which colour the user wants to play before starting a training game.
class NewTrainingDialog : public QDialog {
    Q_OBJECT

public:
    /// `lastSide` is the colour preselected, usually the one played last time.
    explicit NewTrainingDialog(Side lastSide, QWidget *parent = nullptr);

    /// The chosen colour; "Random" is resolved here, so every call may differ.
    Side side() const;

private:
    QRadioButton *m_white;
    QRadioButton *m_black;
    QRadioButton *m_random;
};
