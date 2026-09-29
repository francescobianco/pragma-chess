#include "DatabaseSettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

DatabaseSettingsDialog::DatabaseSettingsDialog(const QString &databaseName, const DatabaseProperties &properties,
                                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Database Settings"));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->addRow(tr("Database:"), new QLabel(databaseName, this));

    m_type = new QComboBox(this);
    m_type->addItem(tr("Game Collection"), QVariant::fromValue(int(DatabaseType::GameCollection)));
    m_type->addItem(tr("Opening Book"), QVariant::fromValue(int(DatabaseType::OpeningBook)));
    m_type->setCurrentIndex(m_type->findData(int(properties.type)));
    form->addRow(tr("Database &type:"), m_type);

    auto *typeHelp = new QLabel(tr("An opening book names openings and variations: each game is a line, "
                                   "Event is its name and ECO its code. Only opening books are offered "
                                   "in Book ▸ Opening Names."),
                                this);
    typeHelp->setWordWrap(true);
    typeHelp->setForegroundRole(QPalette::PlaceholderText);
    form->addRow(QString(), typeHelp);

    m_description = new QPlainTextEdit(properties.description, this);
    m_description->setTabChangesFocus(true);
    form->addRow(tr("&Description:"), m_description);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(460, sizeHint().height());
}

DatabaseProperties DatabaseSettingsDialog::properties() const
{
    DatabaseProperties properties;
    properties.type = DatabaseType(m_type->currentData().toInt());
    properties.description = m_description->toPlainText().trimmed();
    return properties;
}
