#pragma once

#include <QDialog>

class QCheckBox;
class QRadioButton;

/// In online play mode "new game" may mean a new online game or a game to
/// analyse: asks which, before the game in progress is kept or resigned.
class NewGameChoiceDialog : public QDialog {
    Q_OBJECT

public:
    enum class Choice { Online, Analysis };

    /// `preselected` is the choice offered; `remembered` says whether it was
    /// asked to be remembered.
    explicit NewGameChoiceDialog(Choice preselected, bool remembered, QWidget *parent = nullptr);

    Choice choice() const;
    /// "Remember for this session": New Game then goes on with this choice
    /// without asking, until the application is closed.
    bool remember() const;

private:
    QRadioButton *m_online;
    QRadioButton *m_analysis;
    QCheckBox *m_remember;
};
