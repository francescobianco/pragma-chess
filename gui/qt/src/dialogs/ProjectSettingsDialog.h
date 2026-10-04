#pragma once

#include <QDialog>

class QLineEdit;

/// File ▸ Project Settings…: what belongs to the project as a whole, such as
/// its name, shown in the title bar in place of the file's.
class ProjectSettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// `fileName` is what the title bar shows without a name.
    ProjectSettingsDialog(const QString &name, const QString &fileName, QWidget *parent = nullptr);

    QString name() const;

private:
    QLineEdit *m_name;
};
