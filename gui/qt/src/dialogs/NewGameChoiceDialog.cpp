#include "NewGameChoiceDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

NewGameChoiceDialog::NewGameChoiceDialog(Choice preselected, bool remembered, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Game"));

    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("You are in online play mode. What kind of new game do you want?"), this);
    label->setWordWrap(true);
    layout->addWidget(label);

    m_online = new QRadioButton(tr("Play a new game &online"), this);
    m_online->setToolTip(tr("As Play Online in the toolbar"));
    m_analysis = new QRadioButton(tr("&Analyse a new game"), this);
    layout->addWidget(m_online);
    layout->addWidget(m_analysis);
    (preselected == Choice::Online ? m_online : m_analysis)->setChecked(true);

    m_remember = new QCheckBox(tr("Remember for this &session"), this);
    m_remember->setToolTip(tr("New Game goes on with this choice without asking, until Pragma Chess is closed."));
    m_remember->setChecked(remembered);
    layout->addSpacing(layout->spacing());
    layout->addWidget(m_remember);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Start"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

NewGameChoiceDialog::Choice NewGameChoiceDialog::choice() const
{
    return m_online->isChecked() ? Choice::Online : Choice::Analysis;
}

bool NewGameChoiceDialog::remember() const
{
    return m_remember->isChecked();
}
