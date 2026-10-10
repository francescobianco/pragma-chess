#include "EnginePanel.h"

#include "FigurineFont.h"
#include "EvaluationBox.h"
#include "SectionButton.h"
#include "platform/SymbolicIcons.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

/// Room around the text of the tutor's three choices.
constexpr int kChoicePadding = 10;

} // namespace

EnginePanel::EnginePanel(QAction *analysisAction, QAction *explainAction, QWidget *parent)
    : QWidget(parent)
    , m_tutor(new QWidget)
    , m_tutorMessage(new QLabel)
    , m_lobby(new QWidget)
    , m_lobbyStatus(new QLabel)
    , m_sendMove(new QPushButton(tr("Send Move")))
    , m_sendPlan(new QPushButton(tr("Send Plan")))
    , m_name(new QLabel)
    , m_score(new EvaluationBox)
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
    // The icon in a section of its own, the text centred in the rest; one
    // size for Analyze and Stop, so the button does not jump when it changes.
    auto *toggle = new SectionButton;
    toggle->setDefaultAction(analysisAction);
    toggle->setTexts({tr("Analyze"), tr("Stop")});
    // The eye beside it as tall, and square.
    m_peek->setFixedSize(toggle->height(), toggle->height());
    // Held down, the board shows where the best line ends.
    m_peek->setIcon(SymbolicIcons::icon(QStringLiteral("pragma-eye")));
    m_peek->setAutoRaise(false); // Framed, as the button beside it.
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

    // The score and the game's course, in one box.
    connect(m_score, &EvaluationBox::plyClicked, this, &EnginePanel::coursePlyClicked);
    connect(m_score, &EvaluationBox::viewChanged, this, &EnginePanel::scoreViewChanged);
    layout->addWidget(m_score);

    m_clocks = new ChessClocks(this);
    m_clocks->hide();
    layout->addWidget(m_clocks);

    // Under the clocks, as on a tournament table: draw and resign.
    m_onlineActions = new QWidget(this);
    auto *actions = new QHBoxLayout(m_onlineActions);
    actions->setContentsMargins(0, 0, 0, 0);
    m_draw = new QPushButton(tr("Offer Draw"), m_onlineActions);
    m_resign = new QPushButton(tr("Resign"), m_onlineActions);
    m_draw->setFocusPolicy(Qt::NoFocus);
    m_resign->setFocusPolicy(Qt::NoFocus);
    // One width for both, whichever text the draw button shows.
    int actionWidth = m_resign->sizeHint().width();
    for (const QString &text : {tr("Offer Draw"), tr("Accept Draw"), tr("Draw Offered")}) {
        m_draw->setText(text);
        actionWidth = qMax(actionWidth, m_draw->sizeHint().width());
    }
    m_draw->setText(tr("Offer Draw"));
    m_draw->setFixedWidth(actionWidth + 2 * kChoicePadding);
    m_resign->setFixedWidth(actionWidth + 2 * kChoicePadding);
    actions->addWidget(m_draw);
    actions->addWidget(m_resign);
    actions->addStretch();
    connect(m_draw, &QPushButton::clicked, this, &EnginePanel::drawRequested);
    connect(m_resign, &QPushButton::clicked, this, &EnginePanel::resignRequested);
    m_onlineActions->hide();
    layout->addWidget(m_onlineActions);

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
    QList<QToolButton *> buttons;
    const auto addChoice = [this, choices, &buttons](const QString &text, const QString &toolTip, void (EnginePanel::*chosen)()) {
        auto *button = new QToolButton;
        button->setText(text);
        button->setToolTip(toolTip);
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QToolButton::clicked, this, chosen);
        choices->addWidget(button);
        buttons << button;
    };
    addChoice(tr("Take Back"), tr("Take the move back and play another one"), &EnginePanel::takeBackRequested);
    // Explain is the board's own Explain, not another one: the same action,
    // on and off together with the button under the board.
    auto *explain = new QToolButton;
    explain->setDefaultAction(explainAction);
    explain->setToolButtonStyle(Qt::ToolButtonTextOnly);
    explain->setFocusPolicy(Qt::NoFocus);
    choices->addWidget(explain);
    buttons << explain;
    addChoice(tr("Ignore"), tr("Keep the move: the engine answers"), &EnginePanel::ignoreRequested);
    // One set of three: the same width, the widest text's, with room around it.
    QSize choice;
    for (QToolButton *button : std::as_const(buttons))
        choice = choice.expandedTo(button->sizeHint());
    for (QToolButton *button : std::as_const(buttons))
        button->setFixedSize(choice.width() + 2 * kChoicePadding, choice.height() + kChoicePadding);
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
    m_line->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_line->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // The line takes the room left and never asks for more: a long line
    // would push the tutor's buttons out of the panel (fitLine).
    m_line->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_line->installEventFilter(this);
    layout->addWidget(m_line, 1);

    // Lobby Mode, at the bottom of the panel over the opening and the book:
    // the two sends, large, then what the game waits for.
    auto *lobby = new QVBoxLayout(m_lobby);
    lobby->setContentsMargins(0, 0, 0, 0);
    auto *sends = new QHBoxLayout;
    m_sendMove->setToolTip(tr("Send your next move, the one after where the game stands"));
    m_sendPlan->setToolTip(tr("Send your move and the answers you prepared on the board to your opponent's replies: "
                              "they are played at once when your opponent plays one of them"));
    m_sendMove->setIcon(SymbolicIcons::icon(QStringLiteral("pragma-send-move")));
    m_sendPlan->setIcon(SymbolicIcons::icon(QStringLiteral("pragma-send-plan")));
    for (QPushButton *button : {m_sendMove, m_sendPlan}) {
        const int side = button->fontMetrics().height() + 4;
        button->setIconSize(QSize(side, side));
        button->setMinimumHeight(2 * side + 4); // Room around icon and text: the panel's main action.
        QFont font = button->font();
        font.setBold(true);
        button->setFont(font);
        button->setFocusPolicy(Qt::NoFocus);
        sends->addWidget(button);
    }
    // One width for both, the larger one's with room around: they do not fill the panel.
    const int width = qMax(m_sendMove->sizeHint().width(), m_sendPlan->sizeHint().width()) + 2 * m_sendMove->fontMetrics().height();
    for (QPushButton *button : {m_sendMove, m_sendPlan})
        button->setFixedWidth(width);
    sends->addStretch();
    lobby->addLayout(sends);
    m_lobbyStatus->setFont(FigurineFont::apply(m_lobbyStatus->font()));
    m_lobbyStatus->setWordWrap(true);
    // All its lines, however narrow the panel: a wrapped label is otherwise squeezed under the buttons.
    m_lobbyStatus->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_lobbyStatus->setAccessibleName(tr("Lobby"));
    lobby->addWidget(m_lobbyStatus);
    connect(m_sendMove, &QPushButton::clicked, this, &EnginePanel::sendMoveRequested);
    connect(m_sendPlan, &QPushButton::clicked, this, &EnginePanel::sendPlanRequested);
    m_lobby->hide();
    layout->addWidget(m_lobby);

    // The opening and the book stay at the bottom while the line above changes length.
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setEnabled(false);
    layout->addWidget(separator);

    // One row: the opening on the left, the book on the right.
    auto *context = new QHBoxLayout;
    context->setContentsMargins(0, 0, 0, 0);
    const auto title = [](const QString &text) {
        auto *label = new QLabel(text);
        label->setEnabled(false);
        return label;
    };
    QFont ecoFont = m_eco->font();
    ecoFont.setBold(true);
    m_eco->setFont(ecoFont);
    m_opening->setWordWrap(true);
    m_opening->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_book->setTextInteractionFlags(Qt::TextSelectableByMouse);
    context->addWidget(title(tr("Opening")), 0, Qt::AlignTop);
    context->addWidget(m_eco, 0, Qt::AlignTop);
    context->addWidget(m_opening, 1, Qt::AlignTop);
    context->addSpacing(12);
    context->addWidget(title(tr("Book")), 0, Qt::AlignTop);
    context->addWidget(m_book, 0, Qt::AlignTop);
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

void EnginePanel::setLobby(bool shown, const QString &status, bool canSendMove, bool canSendPlan)
{
    m_lobbyStatus->setText(status);
    m_sendMove->setEnabled(canSendMove);
    m_sendPlan->setEnabled(canSendPlan);
    m_lobby->setVisible(shown);
}

void EnginePanel::setExplanation(const QString &text)
{
    m_explanation->setText(text);
    m_explanation->setVisible(!text.isEmpty());
}

void EnginePanel::setEvaluation(const std::optional<EngineEvaluation> &evaluation, const QString &line, Side mover)
{
    if (!evaluation) {
        m_score->setEvaluation(std::nullopt);
        m_hasLine = false;
        refreshLine();
        return;
    }
    m_hasLine = !evaluation->pv.isEmpty();
    m_score->setEvaluation(evaluation, mover);
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
    if (watched == m_line && event->type() == QEvent::Resize)
        fitLine();
    if (watched == m_peek && m_peek->isEnabled()) {
        // Only a mouse event is read as one.
        const bool mouseEvent = event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseButtonDblClick;
        const bool left = mouseEvent && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton;
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

void EnginePanel::fitLine()
{
    const QString text = m_lineHidden ? tr("The best line is hidden: it is your move.") : m_lineText;
    // On as many lines as the room left holds; when it holds fewer, on one
    // line ending in "…", the whole of it in the tooltip.
    // Wrapped, the rows a little apart: a long line reads better.
    constexpr int kLineHeight = 118; // Per cent.
    const QFontMetrics metrics = m_line->fontMetrics();
    const int width = qMax(1, m_line->width());
    const int rows = metrics.boundingRect(QRect(0, 0, width, 1 << 20), Qt::TextWordWrap, text).height()
                     / qMax(1, metrics.lineSpacing());
    const int needed = rows <= 1 ? metrics.height() : rows * metrics.lineSpacing() * kLineHeight / 100;
    const bool fits = needed <= m_line->height();
    m_line->setWordWrap(fits);
    m_line->setTextFormat(fits ? Qt::RichText : Qt::PlainText);
    m_line->setText(fits ? QStringLiteral("<div style=\"line-height: %1%\">%2</div>").arg(kLineHeight).arg(text.toHtmlEscaped())
                         : metrics.elidedText(text, Qt::ElideRight, width));
    m_line->setToolTip(fits ? QString() : text);
}

void EnginePanel::refreshLine()
{
    fitLine();
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

void EnginePanel::setOnlineActions(bool shown, bool enabled, DrawOffer offer)
{
    m_onlineActions->setVisible(shown);
    m_draw->setText(offer == DrawOffer::Theirs ? tr("Accept Draw")
                    : offer == DrawOffer::Mine ? tr("Draw Offered")
                                               : tr("Offer Draw"));
    m_draw->setToolTip(offer == DrawOffer::Theirs ? tr("Your opponent offers a draw: accept it")
                       : offer == DrawOffer::Mine ? tr("Your offer waits for your opponent's answer")
                                                  : tr("Offer your opponent a draw"));
    m_draw->setEnabled(enabled && offer != DrawOffer::Mine);
    m_resign->setEnabled(enabled);
}

void EnginePanel::setClocks(bool shown, int whiteMs, int blackMs, std::optional<Side> running)
{
    m_clocks->setVisible(shown);
    if (shown)
        m_clocks->setClocks(whiteMs, blackMs, running);
}
