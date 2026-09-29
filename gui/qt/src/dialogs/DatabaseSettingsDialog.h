#pragma once

#include "app/DatabaseProperties.h"

#include <QDialog>

class QComboBox;
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
    QComboBox *m_type;
    QPlainTextEdit *m_description;
};
