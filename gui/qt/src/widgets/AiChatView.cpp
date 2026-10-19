#include "AiChatView.h"

#include <QLineEdit>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

AiChatView::AiChatView(QWidget *parent)
    : QWidget(parent)
    , m_transcript(new QTextBrowser)
    , m_input(new QLineEdit)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_transcript->setOpenExternalLinks(true);
    m_transcript->setAccessibleName(tr("AI chat"));
    m_transcript->setPlaceholderText(tr("Ask ChatGPT about the position on the board."));
    layout->addWidget(m_transcript, 1);
    m_input->setPlaceholderText(tr("Ask ChatGPT…"));
    m_input->setClearButtonEnabled(true);
    connect(m_input, &QLineEdit::returnPressed, this, [this] {
        const QString question = m_input->text().trimmed();
        if (question.isEmpty() || m_waiting)
            return;
        m_input->clear();
        Q_EMIT asked(question);
    });
    layout->addWidget(m_input);
}

void AiChatView::addQuestion(const QString &text)
{
    m_turns << ChatGptResponses::Turn{true, text};
    m_error.clear();
    m_waiting = true;
    render();
}

void AiChatView::appendAnswer(const QString &text)
{
    if (m_turns.isEmpty() || m_turns.last().fromUser)
        m_turns << ChatGptResponses::Turn{false, QString()};
    m_turns.last().text += text;
    render();
}

void AiChatView::finishAnswer()
{
    m_waiting = false;
    render();
}

void AiChatView::showError(const QString &text)
{
    // A question left without an answer goes, so it is not sent again as history.
    if (!m_turns.isEmpty() && m_turns.last().fromUser)
        m_input->setText(m_turns.takeLast().text);
    m_error = text;
    m_waiting = false;
    render();
}

void AiChatView::clear()
{
    m_turns.clear();
    m_error.clear();
    m_waiting = false;
    render();
}

void AiChatView::render()
{
    // The questions in bold, the answers as ChatGPT writes them (Markdown).
    QString markdown;
    for (const ChatGptResponses::Turn &turn : std::as_const(m_turns)) {
        if (turn.fromUser) {
            QString question = turn.text;
            question.replace(QLatin1Char('\n'), QLatin1Char(' '));
            markdown += QStringLiteral("**") + question.toHtmlEscaped() + QStringLiteral("**\n\n");
        } else {
            markdown += turn.text + QStringLiteral("\n\n");
        }
    }
    if (m_waiting && (m_turns.isEmpty() || m_turns.last().fromUser))
        markdown += QStringLiteral("…\n\n");
    if (!m_error.isEmpty())
        markdown += QStringLiteral("*") + m_error.toHtmlEscaped() + QStringLiteral("*\n\n");
    m_transcript->setMarkdown(markdown);
    m_transcript->verticalScrollBar()->setValue(m_transcript->verticalScrollBar()->maximum());
}
