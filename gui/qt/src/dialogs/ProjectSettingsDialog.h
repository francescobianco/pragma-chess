#pragma once

#include "app/LocalizedText.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

/// File ▸ Project Settings…: what belongs to the project as a whole — its
/// name, shown in the title bar in place of the file's, and its languages.
/// A multilingual project has its texts (name, chapters' titles, titles,
/// subtitles and paragraphs) in several languages, the structure the same in
/// all: the language they are shown and written in is chosen here. Any other
/// project is shown and written in the language of the interface.
class ProjectSettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// `filePath` is the project's file, absolute (empty for a project never
    /// saved), shown read-only; `fileName` is what the title bar shows without
    /// a name; `language` the
    /// language the texts are written in now, `interfaceLanguage` the one a
    /// project that is not multilingual is written in.
    ProjectSettingsDialog(const LocalizedText &name, const QString &filePath, const QString &fileName, bool multilingual,
                          const QString &language, const QString &interfaceLanguage, bool readOnly,
                          QWidget *parent = nullptr);

    /// The name, with what was written in each language.
    LocalizedText name() const;
    bool isMultilingual() const;
    /// The project is protected from changes made without thinking.
    bool isReadOnly() const;
    /// The language the project's texts are shown and written in from now on.
    QString language() const;

private:
    /// Shows the name in the language chosen, keeping what was written in the one before.
    void showLanguage();

    QLineEdit *m_name;
    QCheckBox *m_multilingual;
    QCheckBox *m_readOnly;
    QComboBox *m_language;
    QString m_interfaceLanguage;
    LocalizedText m_names;
    /// The language the name field shows.
    QString m_shown;
};
