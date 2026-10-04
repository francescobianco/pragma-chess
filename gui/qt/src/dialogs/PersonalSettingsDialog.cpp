#include "PersonalSettingsDialog.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

PersonalSettingsDialog::PersonalSettingsDialog(const PersonalSettings &settings, QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(settings.name, this))
    , m_birthYear(new QSpinBox(this))
    , m_fideId(new QLineEdit(settings.fideId, this))
{
    setWindowTitle(tr("Personal Settings"));
    setMinimumWidth(460);

    m_name->setPlaceholderText(tr("As it should appear in your games, e.g. Rossi, Mario"));
    m_name->setClearButtonEnabled(true);
    // The lowest value shows as a dash: no year given.
    m_birthYear->setRange(1899, QDate::currentDate().year());
    m_birthYear->setSpecialValueText(QStringLiteral("—"));
    m_birthYear->setValue(settings.birthYear > 0 ? settings.birthYear : m_birthYear->minimum());
    m_fideId->setPlaceholderText(tr("Digits only, e.g. 896489"));
    m_fideId->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,12}")), m_fideId));

    auto *form = new QFormLayout;
    form->addRow(tr("My &name:"), m_name);
    form->addRow(tr("Year of &birth:"), m_birthYear);
    form->addRow(tr("&FIDE ID:"), m_fideId);

    auto *note = new QLabel(tr("Your name goes on your side of new games and training games, unless the open "
                               "database already knows you: a player marked as Me with Who Is This? wins. These "
                               "settings are kept in .pragma-chess.conf in your Pragma folder, which Sync carries "
                               "to your other computers."),
                            this);
    note->setWordWrap(true);
    note->setEnabled(false); // Greyed: a remark, not a setting.

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addStretch();
    layout->addWidget(buttons);
}

PersonalSettings PersonalSettingsDialog::settings() const
{
    PersonalSettings settings;
    settings.name = m_name->text().trimmed();
    settings.birthYear = m_birthYear->value() > m_birthYear->minimum() ? m_birthYear->value() : 0;
    settings.fideId = m_fideId->text().trimmed();
    return settings;
}
