#include "DatabaseSettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QVBoxLayout>

DatabaseSettingsDialog::DatabaseSettingsDialog(const QString &databaseName, const DatabaseProperties &properties,
                                               QWidget *parent)
    : QDialog(parent)
    , m_properties(properties)
{
    setWindowTitle(tr("Database Settings"));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->addRow(tr("Database:"), new QLabel(databaseName, this));

    m_name = new QLineEdit(properties.name, this);
    m_name->setPlaceholderText(databaseName);
    m_name->setToolTip(tr("The name shown for this database; empty to show the file name"));
    form->addRow(tr("&Name:"), m_name);
    if (!properties.localizedNames.isEmpty()) {
        // Translations of the name are shown, not edited: they come with the
        // databases we ship.
        QStringList codes = properties.localizedNames.keys();
        codes.sort();
        QStringList lines;
        for (const QString &code : std::as_const(codes)) {
            const QString language = QLocale::languageToString(QLocale(code).language());
            lines << tr("%1: %2").arg(language, properties.localizedNames.value(code));
        }
        auto *translations = new QLabel(lines.join(QLatin1Char('\n')), this);
        translations->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(tr("Translations:"), translations);
    }

    m_type = new QComboBox(this);
    m_type->addItem(tr("Game Collection"), QVariant::fromValue(int(DatabaseType::GameCollection)));
    m_type->addItem(tr("Opening Book"), QVariant::fromValue(int(DatabaseType::OpeningBook)));
    m_type->setCurrentIndex(m_type->findData(int(properties.type)));
    form->addRow(tr("Database &type:"), m_type);

    auto *typeHelp = new QLabel(tr("An opening book names openings and variations: each game is a line, "
                                   "Event is its name and ECO its code. Only opening books are offered "
                                   "in Options ▸ Opening Names."),
                                this);
    typeHelp->setWordWrap(true);
    typeHelp->setForegroundRole(QPalette::PlaceholderText);
    form->addRow(QString(), typeHelp);

    m_description = new QPlainTextEdit(properties.description, this);
    m_description->setTabChangesFocus(true);
    form->addRow(tr("&Description:"), m_description);
    if (!properties.id.isEmpty()) {
        // Read only: it is what makes copies on other devices the same database.
        auto *id = new QLabel(properties.id, this);
        id->setTextInteractionFlags(Qt::TextSelectableByMouse);
        id->setToolTip(tr("Copies of this database on other computers and phones share this id, "
                          "whatever the file is called, so their games are merged when syncing."));
        form->addRow(tr("Universal id:"), id);
    }
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(460, sizeHint().height());
}

DatabaseProperties DatabaseSettingsDialog::properties() const
{
    DatabaseProperties properties = m_properties;
    properties.type = DatabaseType(m_type->currentData().toInt());
    properties.description = m_description->toPlainText().trimmed();
    properties.name = m_name->text().trimmed();
    return properties;
}
