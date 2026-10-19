#pragma once

#include "app/ai/ChatGptResponses.h"

#include <QWidget>

class QLineEdit;
class QTextBrowser;

/// The AI's chat in the Engine panel: the conversation, then the field the
/// question is written in. It only shows and collects; the window asks.
class AiChatView : public QWidget {
    Q_OBJECT

public:
    explicit AiChatView(QWidget *parent = nullptr);

    /// The conversation so far, the question being answered last.
    const QList<ChatGptResponses::Turn> &turns() const { return m_turns; }
    void addQuestion(const QString &text);
    void appendAnswer(const QString &text);
    void finishAnswer();
    /// The answer did not come: said in its place.
    void showError(const QString &text);
    void clear();

Q_SIGNALS:
    void asked(const QString &question);

private:
    void render();

    QTextBrowser *m_transcript;
    QLineEdit *m_input;
    QList<ChatGptResponses::Turn> m_turns;
    QString m_error;
    bool m_waiting = false;
};
