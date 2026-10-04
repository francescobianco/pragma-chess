#pragma once

#include "app/PersonalSettings.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;

/// Options ▸ Personal Settings…: the user's name, year of birth and FIDE ID,
/// kept in the Pragma folder and synced with it.
class PersonalSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit PersonalSettingsDialog(const PersonalSettings &settings, QWidget *parent = nullptr);

    PersonalSettings settings() const;

private:
    QLineEdit *m_name;
    QSpinBox *m_birthYear;
    QLineEdit *m_fideId;
};
