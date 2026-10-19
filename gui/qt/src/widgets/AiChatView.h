#pragma once

#include <QWidget>

class QLineEdit;
class QTextBrowser;

/// The AI's chat in the Engine panel: the conversation, then the field the
/// question is written in. It only shows and collects; AiAssistant asks.
class AiChatView : public QWidget {
    Q_OBJECT

public:
    explicit AiChatView(QWidget *parent = nullptr);

    void addQuestion(const QString &text);
    void appendAnswer(const QString &text);
    /// Something the AI did in Pragma Chess ("Played 1.e4"), said in italics.
    void addAction(const QString &text);
    void finishAnswer();
    /// The answer did not come: said in its place, the question given back to the field.
    void showError(const QString &text);
    void clear();

Q_SIGNALS:
    void asked(const QString &question);

private:
    void render();

    enum class Kind { Question, Answer, Action };
    struct Entry {
        Kind kind;
        QString text;
    };

    QTextBrowser *m_transcript;
    QLineEdit *m_input;
    QList<Entry> m_entries;
    QString m_error;
    bool m_waiting = false;
};
