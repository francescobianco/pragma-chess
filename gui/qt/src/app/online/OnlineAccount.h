#pragma once

#include <QList>
#include <QString>

class QSettings;

/// A connection to a platform the user plays online on: which platform,
/// and as whom. The token lives in SourceCredentials, keyed by `id`, never
/// here.
struct OnlineAccount {
    /// Platforms are named by a key the code knows: "lichess" for now.
    static constexpr char kLichess[] = "lichess";

    QString id;       // A uuid of ours, stable for the token.
    QString platform; // kLichess…
    QString username;

    bool operator==(const OnlineAccount &) const = default;
};

/// The connections of this user, in the user's settings (`online/accounts`):
/// the user's on this computer, never a project's.
class OnlineAccounts {
public:
    static OnlineAccounts load(QSettings &settings);
    void save(QSettings &settings) const;

    const QList<OnlineAccount> &accounts() const { return m_accounts; }
    /// Adds the account, or replaces the one of the same platform and user name.
    void add(const OnlineAccount &account);
    void remove(const QString &id);
    const OnlineAccount *find(const QString &id) const;

    /// The name a platform shows: "lichess.org".
    static QString platformName(const QString &platform);

private:
    QList<OnlineAccount> m_accounts;
};
