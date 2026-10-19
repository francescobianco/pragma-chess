#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>

/// Sign in with ChatGPT (OpenAI's OAuth for open-source apps, 2026-09):
/// the user's ChatGPT plan pays for the AI's requests, no API key. The pure
/// half: the authorization address, the token response, and the
/// credentials kept on this computer. ChatGptSignIn runs the browser flow.
namespace ChatGptAuth {

inline constexpr char kAuthorizeUrl[] = "https://auth.openai.com/api/accounts/authorize";
inline constexpr char kTokenUrl[] = "https://auth.openai.com/api/accounts/oauth/token";
/// The API the plan's tokens are for, asked as the OAuth resource.
inline constexpr char kResource[] = "https://api.openai.com/v1";
/// The client id of a first sign-in: OpenAI registers the app and answers with its own id.
inline constexpr char kDynamicClient[] = "dynamic_agent_client";
/// The scope that lets requests use the plan; without it the user is only known.
inline constexpr char kPlanScope[] = "chatgpt.tokens.use.direct";
/// The callback's port OpenAI's example uses; only the port may change between sign-ins.
inline constexpr quint16 kCallbackPort = 1455;

/// What this computer keeps of the user's ChatGPT sign-in: never synced,
/// never in a project or a database (a file of its own, owner-only).
struct Credentials {
    /// This computer, for OpenAI: "urn:uuid:…", made once and kept.
    QString hostId;
    /// The id OpenAI issued at the first sign-in ("oaiapp_…"), reused after.
    QString clientId;
    QString accessToken;
    QString refreshToken;
    /// Kept to name the account at the next sign-in (id_token_hint).
    QString idToken;
    QStringList scopes;
    QDateTime expiresAt;
    QString email;
    QString name;

    bool isSignedIn() const { return !accessToken.isEmpty(); }
    /// Whether requests may use the user's ChatGPT plan.
    bool canUsePlan() const { return scopes.contains(QLatin1String(kPlanScope)); }
    /// Signed out: the tokens go, the registration and this computer's id stay.
    void signOut();

    QJsonObject toJson() const;
    static Credentials fromJson(const QJsonObject &object);
};

/// The sign-in page's address for these credentials (a first registration
/// when there is no client id yet), returning to `redirect`.
QUrl authorizeUrl(const Credentials &credentials, const QUrl &redirect, const QString &state, const QString &nonce,
                  const QByteArray &codeChallenge, const QString &appName);

/// Takes a token response into `credentials` (tokens, scopes, expiry, the
/// account from the id token); false with `error` when it holds no token.
bool applyTokenResponse(Credentials &credentials, const QJsonObject &response, const QDateTime &now,
                        QString *error = nullptr);

/// The claims of a JWT's payload (not verified: it came straight from OpenAI over TLS).
QJsonObject jwtClaims(const QString &jwt);

/// AppLocalData/chatgpt-auth.json.
QString defaultPath();
Credentials load(const QString &path);
/// Written whole, readable by the owner only.
bool save(const Credentials &credentials, const QString &path);

} // namespace ChatGptAuth
