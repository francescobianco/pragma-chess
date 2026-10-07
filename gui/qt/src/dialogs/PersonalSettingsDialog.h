#pragma once

#include "app/PersonalSettings.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

/// Options ▸ Personal Settings…: the user's name, year of birth and FIDE ID,
/// and the board style, kept in the Pragma folder and synced with it.
class PersonalSettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// `lobbyKey` is this computer's lobby key, as the user copies it; empty
    /// in a build without the lobby, which then shows no such field.
    explicit PersonalSettingsDialog(const PersonalSettings &settings, const QString &lobbyKey = QString(),
                                    QWidget *parent = nullptr);

    PersonalSettings settings() const;
    /// The lobby key in the field: the same, or one the user brought.
    QString lobbyKey() const;

    void accept() override;

private:
    QLineEdit *m_name;
    QSpinBox *m_birthYear;
    QLineEdit *m_fideId;
    QComboBox *m_boardTheme;
    QLineEdit *m_lobbyKey = nullptr;
    QString m_originalLobbyKey;
};
