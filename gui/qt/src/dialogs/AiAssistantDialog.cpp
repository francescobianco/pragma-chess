#include "AiAssistantDialog.h"

#include "app/ai/ChatGptClient.h"
#include "widgets/AiMenu.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {

const QString kModelKey = QStringLiteral("ai/model");

} // namespace

AiAssistantDialog::AiAssistantDialog(AiMenu *account, bool active, QWidget *parent)
    : QDialog(parent)
    , m_account(account)
    , m_client(new ChatGptClient(this))
    , m_logo(new QLabel)
    , m_status(new QLabel)
    , m_signIn(new QPushButton(tr("Sign in with ChatGPT")))
    , m_signOut(new QPushButton(tr("Sign Out")))
    , m_model(new QComboBox)
    , m_modelStatus(new QLabel)
    , m_active(new QCheckBox(tr("AI chat in the Engine panel")))
{
    setWindowTitle(tr("Configure AI Assistant"));
    setMinimumWidth(30 * fontMetrics().averageCharWidth() + 20 * fontMetrics().height());
    auto *layout = new QVBoxLayout(this);

    // The account: OpenAI's logo, who is signed in, and the way in or out,
    // in the form's own grid, so the account lines up with the fields under it.
    auto *form = new QFormLayout;
    auto *who = new QVBoxLayout;
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    who->addWidget(m_status);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_signIn);
    buttons->addWidget(m_signOut);
    buttons->addStretch();
    who->addLayout(buttons);
    form->addRow(m_logo, who);

    m_model->setMinimumContentsLength(24);
    form->addRow(tr("Model:"), m_model);
    m_modelStatus->setForegroundRole(QPalette::PlaceholderText);
    m_modelStatus->setWordWrap(true);
    form->addRow(QString(), m_modelStatus);
    form->addRow(QString(), m_active);
    layout->addLayout(form);

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(close);

    m_active->setChecked(active);
    connect(m_active, &QCheckBox::toggled, this, &AiAssistantDialog::activeChanged);
    connect(m_signIn, &QPushButton::clicked, this, [this] {
        if (m_account->isSigningIn())
            m_account->cancelSignIn();
        else
            m_account->signIn();
    });
    connect(m_signOut, &QPushButton::clicked, m_account, &AiMenu::signOut);
    connect(m_account, &AiMenu::accountChanged, this, &AiAssistantDialog::updateAccount);
    connect(m_model, &QComboBox::activated, this, [this] {
        QSettings().setValue(kModelKey, m_model->currentData().toString());
    });
    connect(m_client, &ChatGptClient::credentialsChanged, m_account, &AiMenu::updateCredentials);
    connect(m_client, &ChatGptClient::signedOut, m_account, &AiMenu::signOut);
    connect(m_client, &ChatGptClient::failed, this, [this](const QString &message) { m_modelStatus->setText(message); });
    connect(m_client, &ChatGptClient::modelsListed, this, [this](const QList<ChatGptResponses::Model> &models) {
        m_listed = true;
        const QString chosen = chosenModel();
        m_model->clear();
        m_model->addItem(tr("Automatic (the first your plan offers)"), QString());
        for (const ChatGptResponses::Model &model : models)
            m_model->addItem(model.name, model.slug);
        // A model chosen before and no longer offered stays chosen, said as such.
        if (!chosen.isEmpty() && m_model->findData(chosen) < 0)
            m_model->addItem(tr("%1 (not offered by your plan now)").arg(chosen), chosen);
        m_model->setCurrentIndex(qMax(0, m_model->findData(chosen)));
        m_model->setEnabled(true);
        m_modelStatus->setText(models.isEmpty() ? tr("Your ChatGPT plan offers no model to Pragma Chess.") : QString());
    });
    updateAccount();
}

QString AiAssistantDialog::chosenModel()
{
    return QSettings().value(kModelKey).toString();
}

void AiAssistantDialog::updateAccount()
{
    const bool white = palette().color(QPalette::WindowText).lightness() > 128;
    m_logo->setPixmap(AiMenu::logo(white, 3 * fontMetrics().height(), devicePixelRatioF()));
    const ChatGptAuth::Credentials &credentials = m_account->credentials();
    const bool signedIn = credentials.isSignedIn();
    QString status;
    if (m_account->isSigningIn()) {
        status = tr("Signing in in the browser…");
    } else if (signedIn) {
        QString account = credentials.name;
        if (!credentials.email.isEmpty())
            account = account.isEmpty() ? credentials.email : tr("%1 (%2)").arg(account, credentials.email);
        status = account.isEmpty() ? tr("Signed in with ChatGPT.") : tr("Signed in with ChatGPT as %1.").arg(account);
        if (!credentials.canUsePlan())
            status += QLatin1Char(' ') + tr("Your ChatGPT plan was not shared with Pragma Chess: sign out and in again to share it.");
    } else {
        status = tr("Not signed in. Your ChatGPT plan pays for the AI's answers: no API key is needed.");
    }
    m_status->setText(status);
    m_signIn->setVisible(!signedIn);
    m_signIn->setText(m_account->isSigningIn() ? tr("Cancel") : tr("Sign in with ChatGPT"));
    m_signOut->setVisible(signedIn);
    m_active->setEnabled(signedIn);
    if (!signedIn) {
        m_listed = false;
        m_client->cancel();
        m_model->clear();
        m_model->addItem(tr("Automatic (the first your plan offers)"), QString());
        m_model->setEnabled(false);
        m_modelStatus->setText(tr("Sign in to choose the model."));
    } else if (!m_listed) {
        listModels();
    }
}

void AiAssistantDialog::listModels()
{
    if (m_model->count() == 0)
        m_model->addItem(tr("Automatic (the first your plan offers)"), QString());
    m_model->setEnabled(false);
    m_modelStatus->setText(tr("Asking ChatGPT for your plan's models…"));
    m_client->setCredentials(m_account->credentials());
    m_client->listModels();
}
