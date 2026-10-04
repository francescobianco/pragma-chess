#pragma once

#include "app/online/LichessBoardClient.h"
#include "app/online/OnlineAccount.h"

#include <QDialog>

class LichessSignIn;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

/// Game ▸ Play Online…: the platforms the user is connected to — each
/// connection an account signed in here, kept per user on this computer,
/// never in a project — and the game to ask for: which connection, clock,
/// rated or not, colour. Connect Platform… asks which kind and signs in.
/// Accepting means "look for an opponent with these".
class PlayOnlineDialog : public QDialog {
    Q_OBJECT

public:
    /// `remembered` says whether the last choice was asked to be remembered.
    explicit PlayOnlineDialog(bool remembered, QWidget *parent = nullptr);
    ~PlayOnlineDialog() override;

    /// The account chosen, once accepted.
    OnlineAccount account() const;
    LichessBoardClient::Seek seek() const;
    /// "Remember for this session": the toolbar's New Online Game then looks for
    /// an opponent with these choices without asking, until the application
    /// is closed.
    bool remember() const;

private:
    void rebuildAccounts();
    void connectPlatform();
    void removeAccount();
    void updateButtons();
    void signedIn(const QString &platform, const QString &token, const QString &username, const QString &error);

    OnlineAccounts m_accounts;
    QListWidget *m_list;
    QPushButton *m_connect;
    QPushButton *m_remove;
    QLabel *m_signInStatus;
    QSpinBox *m_minutes;
    QSpinBox *m_increment;
    QCheckBox *m_rated;
    QComboBox *m_color;
    QCheckBox *m_remember;
    QPushButton *m_play;
    LichessSignIn *m_signIn = nullptr;
};
