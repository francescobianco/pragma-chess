#pragma once

#include "app/GameRecord.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

/// Edits the header information of a game: players, ratings, event, site,
/// date, round, result and ECO code.
class GameInfoDialog : public QDialog {
    Q_OBJECT

public:
    explicit GameInfoDialog(const GameRecord &game, QWidget *parent = nullptr);

    /// The game with the edited header fields (moves are not touched).
    GameRecord game() const;

private:
    GameRecord m_game;
    QLineEdit *m_white;
    QSpinBox *m_whiteElo;
    QLineEdit *m_black;
    QSpinBox *m_blackElo;
    QLineEdit *m_event;
    QLineEdit *m_site;
    QLineEdit *m_date;
    QLineEdit *m_round;
    QComboBox *m_result;
    QLineEdit *m_eco;
};
