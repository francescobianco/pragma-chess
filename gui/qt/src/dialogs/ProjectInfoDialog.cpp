#include "ProjectInfoDialog.h"

#include "platform/SymbolicIcons.h"
#include "widgets/HelpButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

/// A field with its "?" at the end.
QWidget *withHelp(QWidget *field, const QString &help, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(field, 1);
    layout->addWidget(new HelpButton(help, row), 0, Qt::AlignVCenter);
    return row;
}

} // namespace

ProjectInfoDialog::ProjectInfoDialog(const LocalizedText &name, const QString &filePath, const QString &fileName,
                                     bool multilingual, const QString &language, const QString &interfaceLanguage,
                                     bool readOnly, QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(name.text(language), this))
    , m_multilingual(new QCheckBox(tr("&Multilingual project"), this))
    , m_readOnly(new QCheckBox(tr("&Read-only"), this))
    , m_language(new QComboBox(this))
    , m_edit(new QPushButton(this))
    , m_buttons(new QDialogButtonBox(this))
    , m_interfaceLanguage(interfaceLanguage)
    , m_names(name)
    , m_shown(language)
{
    setWindowTitle(tr("Project Information"));
    setMinimumWidth(460);
    m_name->setPlaceholderText(fileName);
    m_name->setClearButtonEnabled(true);
    for (const QString &code : LocalizedText::languages())
        m_language->addItem(LocalizedText::nativeName(code), code);
    m_language->setCurrentIndex(qMax(0, m_language->findData(language)));
    m_multilingual->setChecked(multilingual);
    m_readOnly->setChecked(readOnly);

    // Where the project is, to find it: read-only, selectable to copy, never renamed here.
    auto *file = new QLineEdit(filePath.isEmpty() ? QString() : QDir::toNativeSeparators(filePath), this);
    file->setReadOnly(true);
    file->setPlaceholderText(tr("Not saved yet"));
    file->setCursorPosition(0);

    auto *form = new QFormLayout;
    form->addRow(tr("File:"),
                 withHelp(file,
                          tr("<p>Where the project is kept on this computer. Select it to copy it.</p>"
                             "<p>The file is not renamed here: File ▸ Save Project As… saves the project "
                             "under another name.</p>"),
                          this));
    form->addRow(tr("Project &name:"),
                 withHelp(m_name,
                          tr("<p>Shown in the title bar in place of the file's name, followed by the chapter "
                             "when the project has chapters.</p><p>Empty, the file's name is shown.</p>"),
                          this));
    form->addRow(QString(),
                 withHelp(m_multilingual,
                          tr("<p>A multilingual project has its name, titles, subtitles and paragraphs in several "
                             "languages, its chapters and games the same in all.</p>"
                             "<p>They are shown and written in the language chosen below. A text not written in "
                             "it is shown in English, or in another language that has it.</p>"),
                          this));
    form->addRow(tr("&Language of the texts:"),
                 withHelp(m_language,
                          tr("<p>The language the project's texts are shown and written in.</p>"
                             "<p>A project opens in the language of the interface; one that is not multilingual "
                             "stays in it.</p>"),
                          this));
    form->addRow(QString(),
                 withHelp(m_readOnly,
                          tr("<p>Keeps the project from changes made without thinking: its chapters, titles, "
                             "paragraphs, comments and variations cannot be changed, and it is not saved. The "
                             "board can still be explored.</p>"
                             "<p>Untick it to change the project. The projects distributed with Pragma Chess "
                             "come read-only.</p>"),
                          this));

    connect(m_multilingual, &QCheckBox::toggled, this, [this](bool on) {
        m_language->setEnabled(m_unlocked && on);
        if (!on) // Back to the language of the interface.
            m_language->setCurrentIndex(qMax(0, m_language->findData(m_interfaceLanguage)));
    });
    connect(m_language, &QComboBox::currentIndexChanged, this, &ProjectInfoDialog::showLanguage);

    // Changing the project's information is the exception: unlocked by hand.
    m_edit->setCheckable(true);
    connect(m_edit, &QPushButton::toggled, this, &ProjectInfoDialog::setUnlocked);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(m_edit);
    bottom->addStretch();
    bottom->addWidget(m_buttons);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch();
    layout->addLayout(bottom);
    setUnlocked(false);
}

void ProjectInfoDialog::setUnlocked(bool unlocked)
{
    m_unlocked = unlocked;
    m_name->setReadOnly(!unlocked);
    m_name->setClearButtonEnabled(unlocked);
    m_multilingual->setEnabled(unlocked);
    m_language->setEnabled(unlocked && m_multilingual->isChecked());
    m_readOnly->setEnabled(unlocked);
    m_edit->setText(unlocked ? tr("Editing") : tr("&Edit"));
    m_edit->setIcon(SymbolicIcons::icon(unlocked ? QStringLiteral("pragma-unlocked") : QStringLiteral("pragma-locked")));
    m_edit->setEnabled(!unlocked); // Once open, Cancel puts everything back.
    m_buttons->setStandardButtons(unlocked ? QDialogButtonBox::Ok | QDialogButtonBox::Cancel : QDialogButtonBox::Close);
    if (unlocked)
        m_name->setFocus();
}

LocalizedText ProjectInfoDialog::name() const
{
    LocalizedText names = m_names;
    // Shown as it was, from this language or another one: nothing written.
    if (m_name->text().trimmed() != names.text(m_shown))
        names.set(m_shown, m_name->text().trimmed());
    return names;
}

bool ProjectInfoDialog::isReadOnly() const
{
    return m_readOnly->isChecked();
}

bool ProjectInfoDialog::isMultilingual() const
{
    return m_multilingual->isChecked();
}

QString ProjectInfoDialog::language() const
{
    return m_language->currentData().toString();
}

void ProjectInfoDialog::showLanguage()
{
    m_names = name();
    m_shown = language();
    m_name->setText(m_names.text(m_shown));
}
