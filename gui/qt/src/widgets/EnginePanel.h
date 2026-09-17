#pragma once

#include "app/OpeningNames.h"
#include "app/UciEngine.h"

#include <QWidget>

#include <optional>

class QAction;
class QLabel;

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
    /// Summary of the "Explain" command; empty hides it.
    void setExplanation(const QString &text);
    /// The opening the game is in and the chosen opening book; empty values show a dash.
    void setOpening(const OpeningNames::Name &opening);
    void setBookName(const QString &name);

private:
    QLabel *m_name;
    QLabel *m_score;
    QLabel *m_depth;
    QLabel *m_explanation;
    QLabel *m_line;
    QLabel *m_eco;
    QLabel *m_opening;
    QLabel *m_book;
};
