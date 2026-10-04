#include "FolderSettingsDialog.h"

#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

FolderSettingsDialog::FolderSettingsDialog(const UserFolders::FolderChoice &choice, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Folder Settings"));
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Where Pragma Chess keeps your files on this computer. "
                                "Leave a folder empty to keep it in its usual place."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout;
    m_pragma = addFolder(form, tr("&Pragma folder:"), choice.pragma);
    m_databases = addFolder(form, tr("&Databases:"), choice.databases);
    m_projects = addFolder(form, tr("P&rojects:"), choice.projects);
    m_books = addFolder(form, tr("&Books:"), choice.books);
    m_openingNames = addFolder(form, tr("&Opening names:"), choice.openingNames);
    layout->addLayout(form);

    auto *note = new QLabel(tr("Sync keeps only what is inside the Pragma folder the same on your other "
                               "computers. Files already in a folder are not moved. The new folders are "
                               "used the next time Pragma Chess starts."),
                            this);
    note->setWordWrap(true);
    note->setEnabled(false); // Greyed: a remark, not a setting.
    layout->addWidget(note);

    if (UserFolders::isOverridden()) {
        auto *overridden = new QLabel(tr("PRAGMA_CHESS_DIR is set, so Pragma Chess uses “%1” and ignores "
                                         "these folders while it is.")
                                          .arg(QDir::toNativeSeparators(UserFolders::pragmaDir())),
                                      this);
        overridden->setWordWrap(true);
        layout->addWidget(overridden);
    }

    layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                             | QDialogButtonBox::RestoreDefaults,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        for (QLineEdit *field : {m_pragma, m_databases, m_projects, m_books, m_openingNames})
            field->clear();
    });
    layout->addWidget(buttons);

    for (QLineEdit *field : {m_pragma, m_books})
        connect(field, &QLineEdit::textChanged, this, &FolderSettingsDialog::updatePlaceholders);
    updatePlaceholders();
}

QLineEdit *FolderSettingsDialog::addFolder(QFormLayout *form, const QString &label, const QString &value)
{
    auto *row = new QHBoxLayout;
    auto *field = new QLineEdit(QDir::toNativeSeparators(value), this);
    field->setClearButtonEnabled(true);
    auto *browse = new QPushButton(tr("Choose…"), this);
    connect(browse, &QPushButton::clicked, this, [this, field] {
        const QString start = field->text().isEmpty() ? field->placeholderText() : field->text();
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Choose Folder"), start);
        if (!folder.isEmpty())
            field->setText(QDir::toNativeSeparators(folder));
    });
    row->addWidget(field, 1);
    row->addWidget(browse);
    // The row is a layout: the label needs its buddy to show the shortcut.
    auto *title = new QLabel(label, this);
    title->setBuddy(field);
    form->addRow(title, row);
    return field;
}

void FolderSettingsDialog::updatePlaceholders()
{
    const UserFolders::Folders folders = UserFolders::resolve(choice(), UserFolders::defaultPragmaDir());
    m_pragma->setPlaceholderText(QDir::toNativeSeparators(folders.pragma));
    m_databases->setPlaceholderText(QDir::toNativeSeparators(folders.databases));
    m_projects->setPlaceholderText(QDir::toNativeSeparators(folders.projects));
    m_books->setPlaceholderText(QDir::toNativeSeparators(folders.books));
    m_openingNames->setPlaceholderText(QDir::toNativeSeparators(folders.openingNames));
}

UserFolders::FolderChoice FolderSettingsDialog::choice() const
{
    const auto folder = [](const QLineEdit *field) { return QDir::fromNativeSeparators(field->text().trimmed()); };
    UserFolders::FolderChoice choice;
    choice.pragma = folder(m_pragma);
    choice.databases = folder(m_databases);
    choice.projects = folder(m_projects);
    choice.books = folder(m_books);
    choice.openingNames = folder(m_openingNames);
    return choice;
}
