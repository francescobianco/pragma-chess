#pragma once

#include "app/UciEngine.h"

#include <QWidget>

#include <optional>

class QAction;
class QLabel;

/// Contents of the Engine dock: engine name, score, depth and best line.
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

private:
    QLabel *m_name;
    QLabel *m_score;
    QLabel *m_depth;
    QLabel *m_explanation;
    QLabel *m_line;
};
