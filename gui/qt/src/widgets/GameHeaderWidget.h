#pragma once

#include "app/GameRecord.h"

#include <QWidget>

class QLabel;

/// Title block above the board: "A. Anderssen vs L. Kieseritzky" with the
/// year and tournament underneath. Clicking it edits the game information.
class GameHeaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameHeaderWidget(QWidget *parent = nullptr);

    void setGame(const GameRecord &game, bool editable);

    /// "Anderssen, Adolf" → "A. <b>Anderssen</b>" (HTML, escaped).
    static QString playerNameHtml(const QString &pgnName);

Q_SIGNALS:
    void activated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updateFonts();

    QLabel *m_players;
    QLabel *m_details;
    bool m_editable = false;
    bool m_hovered = false;
};
