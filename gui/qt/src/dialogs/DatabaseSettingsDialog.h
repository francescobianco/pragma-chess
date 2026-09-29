#pragma once

#include "app/DatabaseProperties.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

/// Database ▸ Database Settings…: edits the properties stored in the open
/// database (its type and a description).
class DatabaseSettingsDialog : public QDialog {
    Q_OBJECT

public:
    DatabaseSettingsDialog(const QString &databaseName, const DatabaseProperties &properties,
                           QWidget *parent = nullptr);

    DatabaseProperties properties() const;

private:
    /// What the dialog does not edit (the universal id) is kept as it was.
    DatabaseProperties m_properties;
    QLineEdit *m_name;
    QComboBox *m_type;
    QPlainTextEdit *m_description;
};
