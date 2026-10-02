#pragma once

#include "app/BoardState.h"

#include <QDialog>

class QCheckBox;
class QRadioButton;

/// Asks which colour the user wants to play before starting a training game.
class NewTrainingDialog : public QDialog {
    Q_OBJECT

public:
    enum class Choice { White, Black, Random };

    /// `preselected` is the choice offered, usually the one made last time;
    /// `remembered` says whether it was asked to be remembered.
    explicit NewTrainingDialog(Choice preselected, bool remembered, QWidget *parent = nullptr);

    Choice choice() const;
    /// "Remember for this session": the toolbar's New Training then starts
    /// with this choice without asking, until the application is closed.
    bool remember() const;

    /// The colour of a choice; Random is drawn here, so every call may differ.
    static Side sideFor(Choice choice);

private:
    QRadioButton *m_white;
    QRadioButton *m_black;
    QRadioButton *m_random;
    QCheckBox *m_remember;
};
