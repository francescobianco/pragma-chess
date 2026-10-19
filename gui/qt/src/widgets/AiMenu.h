#pragma once

#include "app/ai/ChatGptAuth.h"

#include <QMenu>

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
    /// Keeps the tokens a refresh brought (ChatGptClient).
    void updateCredentials(const ChatGptAuth::Credentials &credentials);
    /// Forgets the tokens (Sign Out, or a connection ChatGPT no longer honours).
    void signOut();

Q_SIGNALS:
    /// Signed in or out.
    void accountChanged();

private:
    void signIn();
    void cancelSignIn();
    /// The panel as the sign-in stands: the button, or the wait for the browser.
    void updatePanel();

    ChatGptAuth::Credentials m_credentials;
    ChatGptSignIn *m_signIn = nullptr;
    QLabel *m_logo;
    QPushButton *m_signInButton;
    QLabel *m_status;
    QPushButton *m_cancel;
};
