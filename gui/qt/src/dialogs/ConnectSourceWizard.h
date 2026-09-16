#pragma once

#include "app/sources/GameSource.h"

#include <QWizard>

class QLabel;
class QListWidget;
class QVBoxLayout;
class SourceSettingsWidget;

/// Connects an external source of games to a database: choose the source,
/// configure it (differently for each kind), confirm.
class ConnectSourceWizard : public QWizard {
    Q_OBJECT

public:
    ConnectSourceWizard(const QString &databaseName, QWidget *parent = nullptr);
    ~ConnectSourceWizard() override;

    /// The configured source, to add to the database once accepted.
    GameSource source() const;

protected:
    void initializePage(int id) override;
    bool validateCurrentPage() override;
    void done(int result) override;

private:
    enum Page { ChoosePage, SettingsPage, SummaryPage };

    QString chosenKind() const;

    QString m_databaseName;
    QString m_uuid;
    QListWidget *m_kinds;
    QVBoxLayout *m_settingsLayout;
    SourceSettingsWidget *m_settings = nullptr;
    QString m_settingsKind;
    QLabel *m_settingsError;
    QLabel *m_summary;
};
