#include "NewTrainingDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QVBoxLayout>

NewTrainingDialog::NewTrainingDialog(Side lastSide, QWidget *parent)
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
    (lastSide == Side::White ? m_white : m_black)->setChecked(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Start"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

Side NewTrainingDialog::side() const
{
    if (m_random->isChecked())
        return QRandomGenerator::global()->bounded(2) == 0 ? Side::White : Side::Black;
    return m_white->isChecked() ? Side::White : Side::Black;
}
