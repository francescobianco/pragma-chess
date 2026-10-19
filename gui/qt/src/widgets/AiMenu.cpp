#include "AiMenu.h"

#include "app/ai/ChatGptSignIn.h"

#include <QDesktopServices>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#ifdef PRAGMA_HAS_SVG
#include <QSvgRenderer>
#endif


AiMenu::AiMenu(QWidget *parent)
    : QMenu(parent)
    , m_credentials(ChatGptAuth::load(ChatGptAuth::defaultPath()))
    , m_logo(new QLabel)
    , m_signInButton(new QPushButton(tr("Sign in with ChatGPT")))
    , m_status(new QLabel(tr("Signing in in the browser…")))
    , m_cancel(new QPushButton(tr("Cancel")))
{
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    const int room = fontMetrics().height();
    layout->setContentsMargins(room, room, room, room);
    layout->setSpacing(room);
    // The logo large in the middle, the button under it.
    m_logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_logo);

    // OpenAI's button, black; the large logo above it is the only one.
    m_signInButton->setMinimumHeight(2 * fontMetrics().height() + 6);
    m_signInButton->setCursor(Qt::PointingHandCursor);
    m_signInButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #000000; color: #ffffff; border: none; border-radius: 6px;"
        " padding: 0 %1px; font-weight: 600; }"
        "QPushButton:hover { background: #2b2b2b; }"
        "QPushButton:pressed { background: #444444; }").arg(room));
    connect(m_signInButton, &QPushButton::clicked, this, &AiMenu::signIn);
    layout->addWidget(m_signInButton);

    m_status->setAlignment(Qt::AlignCenter);
    m_status->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_status);
    connect(m_cancel, &QPushButton::clicked, this, &AiMenu::cancelSignIn);
    layout->addWidget(m_cancel, 0, Qt::AlignCenter);

    auto *item = new QWidgetAction(this);
    item->setDefaultWidget(panel);
    addAction(item);
    // A click on the AI button while the panel is open only closes it: the
    // press that closed it does not reach the button, which would open it again.
    setAttribute(Qt::WA_NoMouseReplay);
    connect(this, &QMenu::aboutToShow, this, &AiMenu::updatePanel);
    updatePanel();
}

void AiMenu::updatePanel()
{
    const bool white = palette().color(QPalette::WindowText).lightness() > 128;
    m_logo->setPixmap(logo(white, 4 * fontMetrics().height(), devicePixelRatioF()));
    const bool waiting = m_signIn != nullptr;
    m_signInButton->setVisible(!waiting);
    m_status->setVisible(waiting);
    m_cancel->setVisible(waiting);
}

void AiMenu::signIn()
{
    m_signIn = new ChatGptSignIn(this);
    connect(m_signIn, &ChatGptSignIn::openBrowser, this, [](const QUrl &url) { QDesktopServices::openUrl(url); });
    connect(m_signIn, &ChatGptSignIn::finished, this,
            [this](const ChatGptAuth::Credentials &credentials, const QString &error) {
                m_signIn->deleteLater();
                m_signIn = nullptr;
                if (error.isEmpty()) {
                    m_credentials = credentials;
                } else {
                    // The client id OpenAI issued stays, even when the rest failed.
                    if (m_credentials.clientId.isEmpty())
                        m_credentials.clientId = credentials.clientId;
                    QMessageBox::warning(parentWidget(), tr("ChatGPT"), error);
                }
                ChatGptAuth::save(m_credentials, ChatGptAuth::defaultPath());
                updatePanel();
                Q_EMIT accountChanged();
            });
    // This computer's id is kept before the first sign-in, as OpenAI asks.
    ChatGptAuth::save(m_credentials, ChatGptAuth::defaultPath());
    updatePanel();
    close(); // The browser comes forward; the panel says it waits when opened again.
    m_signIn->start(m_credentials);
    Q_EMIT accountChanged();
}

QPixmap AiMenu::logo(bool white, int side, qreal ratio)
{
    QPixmap pixmap(QSize(side, side) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
#ifdef PRAGMA_HAS_SVG
    QSvgRenderer renderer(white ? QStringLiteral(":/ai/chatgpt-logo-white.svg")
                                : QStringLiteral(":/ai/chatgpt-logo-black.svg"));
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, side, side));
#else
    Q_UNUSED(white)
#endif
    return pixmap;
}

void AiMenu::cancelSignIn()
{
    if (!m_signIn)
        return;
    m_signIn->cancel();
    m_signIn->deleteLater();
    m_signIn = nullptr;
    updatePanel();
    Q_EMIT accountChanged();
}

void AiMenu::updateCredentials(const ChatGptAuth::Credentials &credentials)
{
    m_credentials = credentials;
    ChatGptAuth::save(m_credentials, ChatGptAuth::defaultPath());
}

void AiMenu::signOut()
{
    m_credentials.signOut();
    ChatGptAuth::save(m_credentials, ChatGptAuth::defaultPath());
    Q_EMIT accountChanged();
}
