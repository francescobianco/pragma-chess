#include "ProjectInfoDialog.h"

#include "platform/SymbolicIcons.h"
#include "widgets/HelpButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
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

ProjectInfoDialog::ProjectInfoDialog(const LocalizedText &name, const Project::Details &details, const QString &filePath,
                                     const QString &fileName, bool multilingual, const QString &language, bool readOnly,
                                     QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(name.text(language), this))
    , m_description(new QLineEdit(details.description.text(language), this))
    , m_author(new QLineEdit(details.author, this))
    , m_contacts(new QLineEdit(details.contacts, this))
    , m_edition(new QLineEdit(details.edition, this))
    , m_descriptions(details.description)
    , m_multilingual(new QCheckBox(tr("&Multilingual project"), this))
    , m_readOnly(new QCheckBox(tr("&Read-only"), this))
    , m_language(new QComboBox(this))
    , m_edit(new QPushButton(this))
    , m_buttons(new QDialogButtonBox(this))
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

    // What the window is for, before the fields.
    auto *intro = new QLabel(tr("Here you find what the project is: the file it is kept in, its name, what it is "
                                "about, who made it and which edition it is, the language of its texts, then how it "
                                "behaves. These are seldom changed: Edit unlocks them."),
                             this);
    intro->setWordWrap(true);

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
    form->addRow(tr("&Description:"),
                 withHelp(m_description,
                          tr("<p>What the project is about, in a line: a study of the Italian Game, the games "
                             "of a tournament, a course for beginners.</p>"
                             "<p>In a multilingual project it is written in each language, like the name.</p>"),
                          this));
    form->addRow(tr("&Author:"),
                 withHelp(m_author,
                          tr("<p>Who made the project: a name, a club, a school.</p>"), this));
    form->addRow(tr("&Contacts:"),
                 withHelp(m_contacts,
                          tr("<p>How to reach the author: an email address, a web site, a telephone "
                             "number — whatever they wish to give.</p>"),
                          this));
    form->addRow(tr("Editio&n:"),
                 withHelp(m_edition,
                          tr("<p>Which edition of the project this is, written freely: “2nd edition”, "
                             "“October 2026”, “v1.3”.</p>"),
                          this));
    form->addRow(tr("&Language of the texts:"),
                 withHelp(m_language,
                          tr("<p>In a project in one language, the language it is written in: what it "
                             "declares, and where its texts go, whatever the language of the interface. Choosing "
                             "another declares the same texts in it; they are not translated.</p>"
                             "<p>In a multilingual project, the language you work in: its texts are shown and "
                             "written in it. It opens in the language of the interface.</p>"),
                          this));

    // The flags, apart: how the project behaves, under what it is.
    auto *flags = new QVBoxLayout;
    flags->setSpacing(6);
    flags->addWidget(withHelp(m_multilingual,
                              tr("<p>A multilingual project has its name, titles, subtitles and paragraphs in several "
                                 "languages, its chapters and games the same in all.</p>"
                                 "<p>They are shown and written in the Language of the texts chosen above. A text not "
                                 "written in it is shown in English, or in another language that has it.</p>"),
                              this));
    flags->addWidget(withHelp(m_readOnly,
                              tr("<p>Keeps the project from changes made without thinking: its chapters, titles, "
                                 "paragraphs, comments and variations cannot be changed, and it is not saved. The "
                                 "board can still be explored.</p>"
                                 "<p>Untick it to change the project. The projects distributed with Pragma Chess "
                                 "come read-only.</p>"),
                              this));

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

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addSpacing(6);
    layout->addLayout(form);
    layout->addSpacing(4);
    layout->addWidget(line);
    layout->addLayout(flags);
    layout->addSpacing(18); // Room under the flags before the window's buttons.
    layout->addStretch();
    layout->addLayout(bottom);
    setUnlocked(false);
}

void ProjectInfoDialog::setUnlocked(bool unlocked)
{
    m_unlocked = unlocked;
    for (QLineEdit *field : {m_name, m_description, m_author, m_contacts, m_edition}) {
        field->setReadOnly(!unlocked);
        field->setClearButtonEnabled(unlocked);
    }
    m_multilingual->setEnabled(unlocked);
    m_language->setEnabled(unlocked); // A declaration, or the language worked in: always there.
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

Project::Details ProjectInfoDialog::details() const
{
    Project::Details details;
    details.description = m_descriptions;
    // As for the name: shown as it was, from this language or another one, nothing written.
    if (m_description->text().trimmed() != details.description.text(m_shown))
        details.description.set(m_shown, m_description->text().trimmed());
    details.author = m_author->text().trimmed();
    details.contacts = m_contacts->text().trimmed();
    details.edition = m_edition->text().trimmed();
    return details;
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
    m_descriptions = details().description;
    if (!m_multilingual->isChecked()) {
        // A declaration: the name stays as it is, now said to be in that language.
        m_shown = language();
        return;
    }
    m_shown = language();
    m_name->setText(m_names.text(m_shown));
    m_description->setText(m_descriptions.text(m_shown));
}
