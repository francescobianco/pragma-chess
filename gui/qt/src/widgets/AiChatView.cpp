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
    m_transcript->setPlaceholderText(tr("Ask ChatGPT about the position, or tell it what to do: "
                                        "\"play your move\", \"find me a game of Fischer's\"."));
    // A few lines of the conversation at least, whatever else the panel shows.
    m_transcript->setMinimumHeight(5 * m_transcript->fontMetrics().lineSpacing());
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
    m_entries << Entry{Kind::Question, text};
    m_error.clear();
    m_waiting = true;
    render();
}

void AiChatView::appendAnswer(const QString &text)
{
    if (m_entries.isEmpty() || m_entries.last().kind != Kind::Answer)
        m_entries << Entry{Kind::Answer, QString()};
    m_entries.last().text += text;
    render();
}

void AiChatView::addAction(const QString &text)
{
    m_entries << Entry{Kind::Action, text};
    render();
}

void AiChatView::finishAnswer()
{
    m_waiting = false;
    render();
}

void AiChatView::showError(const QString &text)
{
    // The question goes back to the field, to be sent again.
    for (qsizetype i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries.at(i).kind == Kind::Question) {
            if (m_input->text().isEmpty())
                m_input->setText(m_entries.at(i).text);
            break;
        }
    }
    m_error = text;
    m_waiting = false;
    render();
}

void AiChatView::clear()
{
    m_entries.clear();
    m_error.clear();
    m_waiting = false;
    render();
}

void AiChatView::render()
{
    // The questions in bold, the answers as ChatGPT writes them (Markdown),
    // what it did in italics.
    QString markdown;
    for (const Entry &entry : std::as_const(m_entries)) {
        QString text = entry.text;
        switch (entry.kind) {
        case Kind::Question:
            markdown += QStringLiteral("**") + text.replace(QLatin1Char('\n'), QLatin1Char(' ')).toHtmlEscaped()
                + QStringLiteral("**\n\n");
            break;
        case Kind::Answer:
            markdown += text + QStringLiteral("\n\n");
            break;
        case Kind::Action:
            markdown += QStringLiteral("*▸ ") + text.toHtmlEscaped() + QStringLiteral("*\n\n");
            break;
        }
    }
    if (m_waiting && (m_entries.isEmpty() || m_entries.last().kind != Kind::Answer))
        markdown += QStringLiteral("…\n\n");
    if (!m_error.isEmpty())
        markdown += QStringLiteral("*") + m_error.toHtmlEscaped() + QStringLiteral("*\n\n");
    m_transcript->setMarkdown(markdown);
    m_transcript->verticalScrollBar()->setValue(m_transcript->verticalScrollBar()->maximum());
}
