#pragma once

#include <QDialog>

class AiMenu;
class ChatGptClient;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

/// Engine ▸ Configure AI Assistant…: the other face of the Engine panel's
/// AI button. The ChatGPT account (sign in, sign out), the model asked, and
/// the chat on or off; each change takes effect at once.
class AiAssistantDialog : public QDialog {
    Q_OBJECT

public:
    AiAssistantDialog(AiMenu *account, bool active, QWidget *parent = nullptr);

    /// The model chosen (QSettings ai/model, per computer), empty for the plan's first.
    static QString chosenModel();

Q_SIGNALS:
    /// The chat turned on or off.
    void activeChanged(bool active);

private:
    void updateAccount();
    void listModels();

    AiMenu *m_account;
    ChatGptClient *m_client;
    QLabel *m_logo;
    QLabel *m_status;
    QPushButton *m_signIn;
    QPushButton *m_signOut;
    QComboBox *m_model;
    QLabel *m_modelStatus;
    QCheckBox *m_active;
    bool m_listed = false;
};
