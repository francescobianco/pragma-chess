#include "EnginePanel.h"

#include <QAction>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

EnginePanel::EnginePanel(QAction *analysisAction, QWidget *parent)
    : QWidget(parent)
    , m_name(new QLabel)
    , m_score(new QLabel)
    , m_depth(new QLabel)
    , m_explanation(new QLabel)
    , m_line(new QLabel)
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
    header->addWidget(m_name, 1);
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

    m_explanation->setWordWrap(true);
    m_explanation->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_explanation->setAccessibleName(tr("Explanation"));
    m_explanation->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_explanation->hide();
    layout->addWidget(m_explanation);

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
        return;
    }
    m_score->setText(evaluation->text());
    m_depth->setText(tr("Depth %1").arg(evaluation->depth));
    m_lineText = line.isEmpty() ? evaluation->pv.mid(0, 12).join(QLatin1Char(' ')) : line;
    refreshLine();
}

void EnginePanel::setLineHidden(bool hidden)
{
    if (m_lineHidden == hidden)
        return;
    m_lineHidden = hidden;
    refreshLine();
}

void EnginePanel::refreshLine()
{
    m_line->setText(m_lineHidden ? tr("The best line is hidden: it is your move.") : m_lineText);
    m_line->setEnabled(!m_lineHidden);
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
