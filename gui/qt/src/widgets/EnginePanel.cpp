#include "EnginePanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

EnginePanel::EnginePanel(QAction *analysisAction, QWidget *parent)
    : QWidget(parent)
    , m_name(new QLabel)
    , m_score(new QLabel)
    , m_depth(new QLabel)
    , m_line(new QLabel)
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

    m_line->setWordWrap(true);
    m_line->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_line->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_line, 1);

    setEvaluation(std::nullopt);
}

void EnginePanel::setEngineName(const QString &name)
{
    m_name->setText(name);
}

void EnginePanel::setStatus(const QString &status)
{
    m_line->setText(status);
}

void EnginePanel::setEvaluation(const std::optional<EngineEvaluation> &evaluation)
{
    if (!evaluation) {
        m_score->setText(QStringLiteral("–"));
        m_depth->clear();
        return;
    }
    m_score->setText(evaluation->text());
    m_depth->setText(tr("Depth %1").arg(evaluation->depth));
    // TODO: show SAN once the database engine can convert moves.
    m_line->setText(evaluation->pv.mid(0, 12).join(QLatin1Char(' ')));
}
