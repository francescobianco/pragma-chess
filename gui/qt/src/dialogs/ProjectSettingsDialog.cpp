#include "ProjectSettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

ProjectSettingsDialog::ProjectSettingsDialog(const QString &name, const QString &fileName, QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(name, this))
{
    setWindowTitle(tr("Project Settings"));
    setMinimumWidth(420);
    m_name->setPlaceholderText(fileName);
    m_name->setClearButtonEnabled(true);

    auto *form = new QFormLayout;
    form->addRow(tr("Project &name:"), m_name);
    auto *note = new QLabel(tr("Shown in the title bar in place of the file's name, followed by the chapter "
                               "when the project has more than one. Empty, the file's name is shown."),
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

QString ProjectSettingsDialog::name() const
{
    return m_name->text().trimmed();
}
