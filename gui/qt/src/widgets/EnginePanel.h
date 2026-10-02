#pragma once

#include "app/OpeningNames.h"
#include "app/UciEngine.h"

#include <QWidget>

#include <optional>

class QAction;
class QLabel;
class QToolButton;

/// Contents of the Engine dock: engine name, score, depth and best line, and
/// under them the opening of the game and the opening book in use.
class EnginePanel : public QWidget {
    Q_OBJECT

public:
    EnginePanel(QAction *analysisAction, QWidget *parent = nullptr);

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
    /// Summary of the "Explain" command; empty hides it.
    void setExplanation(const QString &text);
    /// The opening the game is in and the chosen opening book; empty values show a dash.
    void setOpening(const OpeningNames::Name &opening);
    void setBookName(const QString &name);

Q_SIGNALS:
    /// The choices of the tutor's alert.
    void takeBackRequested();
    void explainRequested();
    void ignoreRequested();

private:
    QWidget *m_tutor;
    QLabel *m_tutorMessage;
    QLabel *m_name;
    QLabel *m_score;
    QLabel *m_depth;
    QLabel *m_explanation;
    QLabel *m_line;
    QLabel *m_eco;
    QLabel *m_opening;
    QLabel *m_book;
    /// What m_line would show if it were not hidden.
    QString m_lineText;
    bool m_lineHidden = false;

    void refreshLine();
};
