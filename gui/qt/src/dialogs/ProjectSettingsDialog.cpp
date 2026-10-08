#include "ProjectSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

ProjectSettingsDialog::ProjectSettingsDialog(const LocalizedText &name, const QString &filePath, const QString &fileName,
                                             bool multilingual,
                                             const QString &language, const QString &interfaceLanguage,
                                             bool readOnly, QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(name.text(language), this))
    , m_multilingual(new QCheckBox(tr("&Multilingual project"), this))
    , m_readOnly(new QCheckBox(tr("&Read-only"), this))
    , m_language(new QComboBox(this))
    , m_interfaceLanguage(interfaceLanguage)
    , m_names(name)
    , m_shown(language)
{
    setWindowTitle(tr("Project Settings"));
    setMinimumWidth(460);
    m_name->setPlaceholderText(fileName);
    m_name->setClearButtonEnabled(true);
    for (const QString &code : LocalizedText::languages())
        m_language->addItem(LocalizedText::nativeName(code), code);
    m_language->setCurrentIndex(qMax(0, m_language->findData(language)));
    m_multilingual->setChecked(multilingual);
    m_language->setEnabled(multilingual);

    // Where the project is, to find it: read-only, selectable to copy.
    auto *file = new QLineEdit(filePath.isEmpty() ? QString() : QDir::toNativeSeparators(filePath), this);
    file->setReadOnly(true);
    file->setPlaceholderText(tr("Not saved yet"));
    file->setCursorPosition(0);

    auto *form = new QFormLayout;
    form->addRow(tr("File:"), file);
    form->addRow(tr("Project &name:"), m_name);
    auto *note = new QLabel(tr("Shown in the title bar in place of the file's name, followed by the chapter "
                               "when the project has more than one. Empty, the file's name is shown."),
                            this);
    note->setWordWrap(true);
    note->setEnabled(false); // Greyed: a remark, not a setting.
    form->addRow(QString(), note);
    form->addRow(QString(), m_multilingual);
    form->addRow(tr("&Language of the texts:"), m_language);
    auto *languageNote = new QLabel(tr("A multilingual project has its name, titles, subtitles and paragraphs in "
                                       "several languages, its chapters and games the same in all: they are shown "
                                       "and written in the language chosen here. A text not written in it is "
                                       "shown in English, or in another language that has it. A project opens in "
                                       "the language of the interface; one that is not multilingual stays in it."),
                                    this);
    languageNote->setWordWrap(true);
    languageNote->setEnabled(false);
    form->addRow(QString(), languageNote);
    m_readOnly->setChecked(readOnly);
    form->addRow(QString(), m_readOnly);
    auto *readOnlyNote = new QLabel(tr("Keeps the project from changes made without thinking: its chapters, "
                                       "titles, paragraphs, comments and variations cannot be changed, and it is "
                                       "not saved. The board can still be explored. Untick it to change the "
                                       "project; the projects distributed with Pragma Chess come read-only."),
                                    this);
    readOnlyNote->setWordWrap(true);
    readOnlyNote->setEnabled(false);
    form->addRow(QString(), readOnlyNote);

    connect(m_multilingual, &QCheckBox::toggled, this, [this](bool on) {
        m_language->setEnabled(on);
        if (!on) // Back to the language of the interface.
            m_language->setCurrentIndex(qMax(0, m_language->findData(m_interfaceLanguage)));
    });
    connect(m_language, &QComboBox::currentIndexChanged, this, &ProjectSettingsDialog::showLanguage);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(buttons);
}

LocalizedText ProjectSettingsDialog::name() const
{
    LocalizedText names = m_names;
    // Shown as it was, from this language or another one: nothing written.
    if (m_name->text().trimmed() != names.text(m_shown))
        names.set(m_shown, m_name->text().trimmed());
    return names;
}

bool ProjectSettingsDialog::isReadOnly() const
{
    return m_readOnly->isChecked();
}

bool ProjectSettingsDialog::isMultilingual() const
{
    return m_multilingual->isChecked();
}

QString ProjectSettingsDialog::language() const
{
    return m_language->currentData().toString();
}

void ProjectSettingsDialog::showLanguage()
{
    m_names = name();
    m_shown = language();
    m_name->setText(m_names.text(m_shown));
}
