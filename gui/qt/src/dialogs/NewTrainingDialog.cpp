#include "NewTrainingDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QVBoxLayout>

NewTrainingDialog::NewTrainingDialog(Choice preselected, bool remembered, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Training"));

    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("Choose the colour you want to play. The engine plays the other side."), this);
    label->setWordWrap(true);
    layout->addWidget(label);

    m_white = new QRadioButton(tr("&White"), this);
    m_black = new QRadioButton(tr("&Black"), this);
    m_random = new QRadioButton(tr("&Random"), this);
    layout->addWidget(m_white);
    layout->addWidget(m_black);
    layout->addWidget(m_random);
    (preselected == Choice::White ? m_white : preselected == Choice::Black ? m_black : m_random)->setChecked(true);

    m_remember = new QCheckBox(tr("Remember for this &session"), this);
    m_remember->setToolTip(tr("The New Training button of the toolbar starts with this choice without asking, "
                              "until Pragma Chess is closed. Game ▸ New Training… always asks."));
    m_remember->setChecked(remembered);
    layout->addSpacing(layout->spacing());
    layout->addWidget(m_remember);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Start"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

NewTrainingDialog::Choice NewTrainingDialog::choice() const
{
    if (m_random->isChecked())
        return Choice::Random;
    return m_white->isChecked() ? Choice::White : Choice::Black;
}

bool NewTrainingDialog::remember() const
{
    return m_remember->isChecked();
}

Side NewTrainingDialog::sideFor(Choice choice)
{
    if (choice == Choice::Random)
        return QRandomGenerator::global()->bounded(2) == 0 ? Side::White : Side::Black;
    return choice == Choice::White ? Side::White : Side::Black;
}
