#include "EnginePanel.h"

#include "FigurineFont.h"
#include "platform/SymbolicIcons.h"

#include <QAction>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>

EnginePanel::EnginePanel(QAction *analysisAction, QWidget *parent)
    : QWidget(parent)
    , m_tutor(new QWidget)
    , m_tutorMessage(new QLabel)
    , m_name(new QLabel)
    , m_score(new QLabel)
    , m_depth(new QLabel)
    , m_explanation(new QLabel)
    , m_line(new QLabel)
    , m_peek(new QToolButton)
    , m_eco(new QLabel)
    , m_opening(new QLabel)
    , m_book(new QLabel)
{
    auto *layout = new QVBoxLayout(this);

    auto *header = new QHBoxLayout;
    QFont nameFont = m_name->font();
    nameFont.setBold(true);
    m_name->setFont(nameFont);
    auto *toggle = new QToolButton;
    toggle->setDefaultAction(analysisAction);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setAutoRaise(true);
    // Held down, the board shows where the best line ends.
    m_peek->setIcon(SymbolicIcons::icon(QStringLiteral("pragma-eye")));
    m_peek->setAutoRaise(true);
    m_peek->setToolTip(tr("Hold: where the line ends"));
    m_peek->setEnabled(false);
    // The physical press and release, not the button's own down state, which
    // taking the focus let go after a moment.
    m_peek->setFocusPolicy(Qt::NoFocus);
    m_peek->installEventFilter(this);
    header->addWidget(m_name, 1);
    header->addWidget(m_peek);
    header->addWidget(toggle);
    layout->addLayout(header);

    auto *scoreRow = new QHBoxLayout;
    QFont scoreFont = m_score->font();
    scoreFont.setPointSizeF(scoreFont.pointSizeF() * 1.8);
    scoreFont.setBold(true);
    m_score->setFont(scoreFont);
    m_depth->setEnabled(false);
    scoreRow->addWidget(m_score);
    scoreRow->addStretch();
    scoreRow->addWidget(m_depth, 0, Qt::AlignBottom);
    layout->addLayout(scoreRow);

    // The tutor's alert: what went wrong, then what to do about it.
    auto *tutor = new QVBoxLayout(m_tutor);
    tutor->setContentsMargins(0, 0, 0, 0);
    QFont alertFont = m_tutorMessage->font();
    alertFont.setBold(true);
    m_tutorMessage->setFont(FigurineFont::apply(alertFont));
    m_tutorMessage->setWordWrap(true);
    m_tutorMessage->setAccessibleName(tr("Tutor"));
    tutor->addWidget(m_tutorMessage);
    auto *choices = new QHBoxLayout;
    const auto addChoice = [this, choices](const QString &text, const QString &toolTip, void (EnginePanel::*chosen)()) {
        auto *button = new QToolButton;
        button->setText(text);
        button->setToolTip(toolTip);
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QToolButton::clicked, this, chosen);
        choices->addWidget(button);
    };
    addChoice(tr("Take Back"), tr("Take the move back and play another one"), &EnginePanel::takeBackRequested);
    addChoice(tr("Explain"), tr("Show on the board why the move is an error"), &EnginePanel::explainRequested);
    addChoice(tr("Ignore"), tr("Keep the move: the engine answers"), &EnginePanel::ignoreRequested);
    choices->addStretch();
    tutor->addLayout(choices);
    m_tutor->hide();
    layout->addWidget(m_tutor);

    m_explanation->setWordWrap(true);
    m_explanation->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_explanation->setAccessibleName(tr("Explanation"));
    m_explanation->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_explanation->hide();
    layout->addWidget(m_explanation);

    m_line->setFont(FigurineFont::apply(m_line->font())); // The figurines of the move list.
    m_line->setWordWrap(true);
    m_line->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_line->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_line, 1);

    // The opening and the book stay at the bottom while the line above changes length.
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setEnabled(false);
    layout->addWidget(separator);

    auto *context = new QFormLayout;
    context->setContentsMargins(0, 0, 0, 0);
    context->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    context->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    const auto addRow = [context](const QString &title, QWidget *field) {
        auto *label = new QLabel(title);
        label->setEnabled(false);
        context->addRow(label, field);
    };
    QFont ecoFont = m_eco->font();
    ecoFont.setBold(true);
    m_eco->setFont(ecoFont);
    m_opening->setWordWrap(true);
    m_opening->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *opening = new QHBoxLayout;
    opening->setContentsMargins(0, 0, 0, 0);
    opening->addWidget(m_eco, 0, Qt::AlignTop);
    opening->addWidget(m_opening, 1);
    auto *openingField = new QWidget;
    openingField->setLayout(opening);
    addRow(tr("Opening"), openingField);
    m_book->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addRow(tr("Book"), m_book);
    layout->addLayout(context);

    setEvaluation(std::nullopt);
    setOpening({});
    setBookName(QString());
}

void EnginePanel::setEngineName(const QString &name)
{
    m_name->setText(name);
}

void EnginePanel::setStatus(const QString &status)
{
    m_lineText = status;
    refreshLine();
}

void EnginePanel::setTutorAlert(const QString &message)
{
    m_tutorMessage->setText(message);
    m_tutor->setVisible(!message.isEmpty());
}

void EnginePanel::setExplanation(const QString &text)
{
    m_explanation->setText(text);
    m_explanation->setVisible(!text.isEmpty());
}

void EnginePanel::setEvaluation(const std::optional<EngineEvaluation> &evaluation, const QString &line)
{
    if (!evaluation) {
        m_score->setText(QStringLiteral("–"));
        m_depth->clear();
        m_hasLine = false;
        refreshLine();
        return;
    }
    m_hasLine = !evaluation->pv.isEmpty();
    m_score->setText(evaluation->text());
    m_depth->setText(tr("Depth %1").arg(evaluation->depth));
    m_lineText = line.isEmpty() ? evaluation->pv.join(QLatin1Char(' ')) : line;
    refreshLine();
}

void EnginePanel::setLineHidden(bool hidden)
{
    if (m_lineHidden == hidden)
        return;
    m_lineHidden = hidden;
    refreshLine();
}

bool EnginePanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_peek && m_peek->isEnabled()) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const bool left = event->isPointerEvent() && mouse->button() == Qt::LeftButton;
        // Held down, the board shows the end of the line; let go, it comes
        // back. A quick second press comes as a double click: a press too.
        const bool press = event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick;
        if (press && left) {
            if (!m_peeking) {
                m_peeking = true;
                m_peek->setDown(true);
                Q_EMIT peekHeld(true);
            }
            return true;
        }
        // The press holds the pointer: the release comes here even off the eye.
        if (event->type() == QEvent::MouseButtonRelease && left) {
            stopPeeking();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EnginePanel::stopPeeking()
{
    if (!m_peeking)
        return;
    m_peeking = false;
    m_peek->setDown(false);
    Q_EMIT peekHeld(false);
}

void EnginePanel::refreshLine()
{
    m_line->setText(m_lineHidden ? tr("The best line is hidden: it is your move.") : m_lineText);
    m_line->setEnabled(!m_lineHidden);
    // The end of a line the user may not see is not shown either.
    const bool peekable = m_hasLine && !m_lineHidden;
    if (!peekable)
        stopPeeking();
    m_peek->setEnabled(peekable);
}

void EnginePanel::setOpening(const OpeningNames::Name &opening)
{
    m_eco->setText(opening.eco);
    m_eco->setVisible(!opening.eco.isEmpty());
    m_opening->setText(opening.isEmpty() ? QStringLiteral("–") : opening.name);
}

void EnginePanel::setBookName(const QString &name)
{
    m_book->setText(name.isEmpty() ? QStringLiteral("–") : name);
}
