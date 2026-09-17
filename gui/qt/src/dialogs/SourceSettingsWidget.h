#pragma once

#include "app/sources/GameSource.h"
#include "app/sources/SourceCatalog.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;

/// The settings of a source of one kind: the account, which games to import
/// and, for sources that need it, signing in. Used when connecting a source
/// and when editing it; kinds can grow their own options here over time.
class SourceSettingsWidget : public QWidget {
    Q_OBJECT

public:
    /// `sourceUuid` identifies the source's credentials, even before it is stored.
    SourceSettingsWidget(const SourceKind &kind, const QString &sourceUuid, QWidget *parent = nullptr);

    void setSource(const GameSource &source);
    /// Writes the account and settings into `source`.
    void applyTo(GameSource &source) const;

    /// Checks the account exists on the site and, if needed, that the user
    /// signed in. Blocks with a busy cursor while asking the site.
    bool validate(QString *errorMessage);

    /// Signs in to the site of `kind` in the browser, storing the token under
    /// `sourceUuid`. Returns the account name, or nothing if cancelled or failed.
    static std::optional<QString> signIn(const SourceKind &kind, const QString &sourceUuid, QWidget *parent);

Q_SIGNALS:
    void changed();

private:
    void updateSignInStatus();

    SourceKind m_kind;
    QString m_uuid;
    /// FIDE or FSI, for kinds whose account is a player ID.
    QComboBox *m_idType = nullptr;
    QLineEdit *m_account;
    QCheckBox *m_limitSince;
    QDateEdit *m_since;
    QCheckBox *m_ratedOnly;
    /// The player's name found by validate(), for kinds with a player ID.
    QString m_player;
    QLabel *m_signInStatus = nullptr;
    QPushButton *m_signInButton = nullptr;
};
