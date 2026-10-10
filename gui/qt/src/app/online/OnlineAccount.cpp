#include "OnlineAccount.h"

#include <QSettings>

OnlineAccounts OnlineAccounts::load(QSettings &settings)
{
    OnlineAccounts accounts;
    const int count = settings.beginReadArray(QStringLiteral("online/accounts"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        OnlineAccount account;
        account.id = settings.value(QStringLiteral("id")).toString();
        account.platform = settings.value(QStringLiteral("platform")).toString();
        account.username = settings.value(QStringLiteral("username")).toString();
        if (!account.id.isEmpty() && !account.platform.isEmpty())
            accounts.m_accounts << account;
    }
    settings.endArray();
    return accounts;
}

void OnlineAccounts::save(QSettings &settings) const
{
    settings.beginWriteArray(QStringLiteral("online/accounts"), int(m_accounts.size()));
    for (int i = 0; i < m_accounts.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), m_accounts.at(i).id);
        settings.setValue(QStringLiteral("platform"), m_accounts.at(i).platform);
        settings.setValue(QStringLiteral("username"), m_accounts.at(i).username);
    }
    settings.endArray();
}

void OnlineAccounts::add(const OnlineAccount &account)
{
    for (OnlineAccount &existing : m_accounts) {
        if (existing.platform == account.platform
            && existing.username.compare(account.username, Qt::CaseInsensitive) == 0) {
            existing = account; // Signed in again: the same account, a new token under the new id.
            return;
        }
    }
    m_accounts << account;
}

void OnlineAccounts::remove(const QString &id)
{
    m_accounts.removeIf([&](const OnlineAccount &account) { return account.id == id; });
}

const OnlineAccount *OnlineAccounts::find(const QString &id) const &
{
    for (const OnlineAccount &account : m_accounts) {
        if (account.id == id)
            return &account;
    }
    return nullptr;
}

QString OnlineAccounts::platformName(const QString &platform)
{
    if (platform == QLatin1String(OnlineAccount::kLichess))
        return QStringLiteral("lichess.org");
    if (platform == QLatin1String(OnlineAccount::kFics))
        return QStringLiteral("freechess.org");
    return platform;
}
