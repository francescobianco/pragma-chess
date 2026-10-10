#pragma once

#include "app/OpeningNames.h"
#include "app/UciEngine.h"
#include "widgets/ChessClocks.h"

#include <QWidget>

#include <optional>

class QAction;
class QLabel;
class QPushButton;
class QToolButton;

/// Contents of the Engine dock: engine name, score, depth and best line, and
/// under them the opening of the game and the opening book in use.
class EnginePanel : public QWidget {
    Q_OBJECT

public:
    /// `explainAction` is the board's Explain, offered again by the tutor's alert.
    EnginePanel(QAction *analysisAction, QAction *explainAction, QWidget *parent = nullptr);

    void setEngineName(const QString &name);
    void setStatus(const QString &status);
    /// Score and depth, with the best line already written in SAN.
    void setEvaluation(const std::optional<EngineEvaluation> &evaluation, const QString &line = QString());
    /// Keeps the best line out of sight while still showing the score, so that
    /// training does not give the move away.
    void setLineHidden(bool hidden);
    /// The tutor of a training game: says the move just played was an error
    /// and offers to take it back, to have it explained or to go on. Empty
    /// hides it.
    void setTutorAlert(const QString &message);
    /// Lobby Mode: what the lobby game waits for, and its two sends — the
    /// one move after where the game stands, or the whole plan prepared on
    /// the board. Hidden when not `shown`.
    void setLobby(bool shown, const QString &status = QString(), bool canSendMove = false, bool canSendPlan = false);
    /// Online play: the two clocks, large (see ChessClocks); hidden when not `shown`.
    void setClocks(bool shown, int whiteMs = 0, int blackMs = 0, std::optional<Side> running = std::nullopt);
    /// The clocks follow the board: the colour at its top first.
    void setClocksFlipped(bool flipped) { m_clocks->setFlipped(flipped); }
    /// Summary of the "Explain" command; empty hides it.
    void setExplanation(const QString &text);
    /// The opening the game is in and the chosen opening book; empty values show a dash.
    void setOpening(const OpeningNames::Name &opening);
    void setBookName(const QString &name);
    /// Lets the eye go: the board moved on.
    void cancelPeek() { stopPeeking(); }
    /// Whether the eye is held down now.
    bool isPeeking() const { return m_peeking; }

Q_SIGNALS:
    /// The choices of the tutor's alert.
    void takeBackRequested();
    void ignoreRequested();
    /// The sends of Lobby Mode.
    void sendMoveRequested();
    void sendPlanRequested();
    /// The eye is held down (true) or let go (false): the board shows the
    /// end of the best line meanwhile, and follows it as the engine changes it.
    void peekHeld(bool held);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    ChessClocks *m_clocks;
    QWidget *m_tutor;
    QLabel *m_tutorMessage;
    QWidget *m_lobby;
    QLabel *m_lobbyStatus;
    QPushButton *m_sendMove;
    QPushButton *m_sendPlan;
    QLabel *m_name;
    QLabel *m_score;
    QLabel *m_depth;
    QLabel *m_explanation;
    QLabel *m_line;
    QToolButton *m_peek;
    QLabel *m_eco;
    QLabel *m_opening;
    QLabel *m_book;
    /// What m_line would show if it were not hidden.
    QString m_lineText;
    bool m_lineHidden = false;
    /// Whether the line is about the position on the board (a stale one is not peeked at).
    bool m_hasLine = false;
    /// The eye is held down: the board shows the end of the line.
    bool m_peeking = false;
    void stopPeeking();

    void refreshLine();
};
