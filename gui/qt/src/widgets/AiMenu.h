#pragma once

#include "app/ai/ChatGptAuth.h"

#include <QMenu>
#include <QPixmap>

class ChatGptSignIn;
class QLabel;
class QPushButton;

/// What the Engine panel's AI button drops down while ChatGPT is not
/// connected: OpenAI's logo, and under it the button that signs in with a
/// ChatGPT account. A menu, so it opens, closes and casts its shadow as the
/// others; its one item is the panel. The credentials are this computer's
/// (ChatGptAuth::defaultPath()).
class AiMenu : public QMenu {
    Q_OBJECT

public:
    explicit AiMenu(QWidget *parent = nullptr);

    const ChatGptAuth::Credentials &credentials() const { return m_credentials; }
    bool isSignedIn() const { return m_credentials.isSignedIn(); }
    /// Opens OpenAI's sign-in in the browser (the panel's button, and Configure AI Assistant's).
    void signIn();
    bool isSigningIn() const { return m_signIn != nullptr; }
    void cancelSignIn();
    /// OpenAI's logo, white or black, `side` pixels square at the density `ratio`.
    static QPixmap logo(bool white, int side, qreal ratio);

    /// Keeps the tokens a refresh brought (ChatGptClient).
    void updateCredentials(const ChatGptAuth::Credentials &credentials);
    /// Forgets the tokens (Sign Out, or a connection ChatGPT no longer honours).
    void signOut();

Q_SIGNALS:
    /// Signed in or out, or a sign-in started or given up.
    void accountChanged();

private:
    /// The panel as the sign-in stands: the button, or the wait for the browser.
    void updatePanel();

    ChatGptAuth::Credentials m_credentials;
    ChatGptSignIn *m_signIn = nullptr;
    QLabel *m_logo;
    QPushButton *m_signInButton;
    QLabel *m_status;
    QPushButton *m_cancel;
};
