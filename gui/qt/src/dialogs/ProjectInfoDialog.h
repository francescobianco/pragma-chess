#pragma once

#include "app/LocalizedText.h"
#include "app/Project.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;

/// File ▸ Project Information…: what belongs to the project as a whole — its
/// file, its name (shown in the title bar in place of the file's), its
/// languages and whether it is read-only. Changing it is the exception, not
/// the rule: the fields open locked, and Edit, its padlock unlocked, lets
/// them be changed; the file's name never is (Save Project As does that).
/// A multilingual project has its texts (name, chapters' titles, titles,
/// subtitles and paragraphs) in several languages, the structure the same in
/// all: the language they are shown and written in is chosen here. Any other
/// project is shown and written in the language of the interface. What each
/// field means is behind its "?" (HelpButton), not written under it.
class ProjectInfoDialog : public QDialog {
    Q_OBJECT

public:
    /// `filePath` is the project's file, absolute (empty for a project never
    /// saved), shown read-only; `fileName` is what the title bar shows without
    /// a name; `language` the language the texts are written in now — in a
    /// project in one language, the one it declares.
    ProjectInfoDialog(const LocalizedText &name, const Project::Details &details, const QString &filePath,
                      const QString &fileName, bool multilingual, const QString &language, bool readOnly,
                      QWidget *parent = nullptr);

    /// The name, with what was written in each language.
    LocalizedText name() const;
    /// The description (with what was written in each language), author, contacts, edition.
    Project::Details details() const;
    bool isMultilingual() const;
    /// The project is protected from changes made without thinking.
    bool isReadOnly() const;
    /// In one language, the language the project declares; multilingual, the
    /// one its texts are shown and written in from now on.
    QString language() const;

private:
    /// Shows the name in the language chosen, keeping what was written in the one before.
    void showLanguage();
    /// The fields locked (as the dialog opens) or open to changes.
    void setUnlocked(bool unlocked);

    QLineEdit *m_name;
    QLineEdit *m_description;
    QLineEdit *m_author;
    QLineEdit *m_contacts;
    QLineEdit *m_edition;
    /// The description as it was, in each language.
    LocalizedText m_descriptions;
    QCheckBox *m_multilingual;
    QCheckBox *m_readOnly;
    QComboBox *m_language;
    QPushButton *m_edit;
    QDialogButtonBox *m_buttons;
    LocalizedText m_names;
    /// The language the name field shows.
    QString m_shown;
    bool m_unlocked = false;
};
