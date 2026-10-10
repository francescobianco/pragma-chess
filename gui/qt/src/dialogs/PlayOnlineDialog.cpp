#include "PlayOnlineDialog.h"

#include "app/sources/LichessSignIn.h"
#include "app/sources/SourceCredentials.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QUuid>
#include <QVBoxLayout>

PlayOnlineDialog::PlayOnlineDialog(bool remembered, QWidget *parent)
    : QDialog(parent)
    , m_list(new QListWidget)
    , m_connect(new QPushButton(tr("Connect Platform…")))
    , m_remove(new QPushButton(tr("Disconnect")))
    , m_signInStatus(new QLabel)
    , m_minutes(new QSpinBox)
    , m_increment(new QSpinBox)
    , m_rated(new QCheckBox(tr("Rated game")))
    , m_color(new QComboBox)
    , m_remember(new QCheckBox(tr("Remember for this &session")))
{
    setWindowTitle(tr("Play Online"));
    QSettings settings;
    m_accounts = OnlineAccounts::load(settings);

    auto *layout = new QVBoxLayout(this);

    // The connections: each one a platform signed in to as some account.
    auto *accounts = new QGroupBox(tr("Platforms"));
    auto *accountsLayout = new QVBoxLayout(accounts);
    auto *intro = new QLabel(tr("The platforms you are connected to, each with the account you play as. "
                                "They are yours on this computer: a project does not carry them."));
    intro->setWordWrap(true);
    accountsLayout->addWidget(intro);
    accountsLayout->addWidget(m_list, 1);
    auto *row = new QHBoxLayout;
    row->addWidget(m_connect);
    row->addStretch(1);
    row->addWidget(m_remove);
    accountsLayout->addLayout(row);
    m_signInStatus->setWordWrap(true);
    m_signInStatus->hide();
    accountsLayout->addWidget(m_signInStatus);
    layout->addWidget(accounts, 1);

    // The game to ask for.
    auto *game = new QGroupBox(tr("Game"));
    auto *form = new QFormLayout(game);
    m_minutes->setRange(1, 180);
    m_minutes->setSuffix(tr(" min"));
    m_minutes->setValue(settings.value(QStringLiteral("online/minutes"), 10).toInt());
    m_increment->setRange(0, 180);
    m_increment->setSuffix(tr(" s"));
    m_increment->setValue(settings.value(QStringLiteral("online/increment"), 0).toInt());
    auto *clock = new QHBoxLayout;
    clock->addWidget(m_minutes);
    clock->addWidget(new QLabel(QStringLiteral("+")));
    clock->addWidget(m_increment);
    clock->addStretch(1);
    form->addRow(tr("Clock:"), clock);
    m_color->addItem(tr("Random"), QStringLiteral("random"));
    m_color->addItem(tr("White"), QStringLiteral("white"));
    m_color->addItem(tr("Black"), QStringLiteral("black"));
    m_color->setCurrentIndex(qBound(0, settings.value(QStringLiteral("online/color"), 0).toInt(), 2));
    form->addRow(tr("Play as:"), m_color);
    m_rated->setChecked(settings.value(QStringLiteral("online/rated"), false).toBool());
    form->addRow(QString(), m_rated);
    layout->addWidget(game);

    auto *note = new QLabel(tr("While you play online the engine, Explain and the opening book are off: "
                               "it is you against your opponent."));
    note->setWordWrap(true);
    layout->addWidget(note);
    m_remember->setToolTip(tr("The Play Online button of the toolbar looks for an opponent with these choices "
                              "without asking, until Pragma Chess is closed; the menu always asks"));
    m_remember->setChecked(remembered);
    layout->addWidget(m_remember);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    m_play = buttons->addButton(tr("Find an Opponent"), QDialogButtonBox::AcceptRole);
    m_play->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_connect, &QPushButton::clicked, this, &PlayOnlineDialog::connectPlatform);
    connect(m_remove, &QPushButton::clicked, this, &PlayOnlineDialog::removeAccount);
    connect(m_list, &QListWidget::currentRowChanged, this, &PlayOnlineDialog::updateButtons);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] {
        if (m_play->isEnabled())
            accept();
    });
    connect(this, &QDialog::accepted, this, [this] {
        QSettings settings;
        settings.setValue(QStringLiteral("online/minutes"), m_minutes->value());
        settings.setValue(QStringLiteral("online/increment"), m_increment->value());
        settings.setValue(QStringLiteral("online/rated"), m_rated->isChecked());
        settings.setValue(QStringLiteral("online/color"), m_color->currentIndex());
        settings.setValue(QStringLiteral("online/lastAccount"), account().id);
    });

    rebuildAccounts();
    const QString last = settings.value(QStringLiteral("online/lastAccount")).toString();
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == last)
            m_list->setCurrentRow(i);
    }
    if (m_list->currentRow() < 0 && m_list->count() > 0)
        m_list->setCurrentRow(0);
    updateButtons();
    resize(460, 520);
}

PlayOnlineDialog::~PlayOnlineDialog() = default;

void PlayOnlineDialog::rebuildAccounts()
{
    m_list->clear();
    for (const OnlineAccount &account : m_accounts.accounts()) {
        auto *item = new QListWidgetItem(QStringLiteral("%1 — %2").arg(OnlineAccounts::platformName(account.platform), account.username));
        item->setData(Qt::UserRole, account.id);
        m_list->addItem(item);
    }
    if (m_list->count() == 0) {
        auto *item = new QListWidgetItem(tr("No platform connected yet: connect one to play."));
        item->setFlags(Qt::NoItemFlags);
        m_list->addItem(item);
    }
}

OnlineAccount PlayOnlineDialog::account() const
{
    const QListWidgetItem *item = m_list->currentItem();
    const OnlineAccount *account = item ? m_accounts.find(item->data(Qt::UserRole).toString()) : nullptr;
    return account ? *account : OnlineAccount();
}

bool PlayOnlineDialog::remember() const
{
    return m_remember->isChecked();
}

OnlineClient::Seek PlayOnlineDialog::seek() const
{
    OnlineClient::Seek seek;
    seek.minutes = m_minutes->value();
    seek.increment = m_increment->value();
    seek.rated = m_rated->isChecked();
    seek.color = m_color->currentData().toString();
    return seek;
}

void PlayOnlineDialog::updateButtons()
{
    const bool chosen = !account().id.isEmpty();
    m_remove->setEnabled(chosen);
    m_play->setEnabled(chosen && !m_signIn);
    m_connect->setEnabled(!m_signIn);
}

void PlayOnlineDialog::connectPlatform()
{
    // Which kind of platform, a short menu; then the platform's own sign-in.
    QMenu menu(this);
    for (const char *platform : {OnlineAccount::kLichess, OnlineAccount::kFics}) {
        QAction *action = menu.addAction(OnlineAccounts::platformName(QLatin1String(platform)));
        action->setData(QString::fromLatin1(platform));
    }
    const QAction *chosen = menu.exec(m_connect->mapToGlobal(QPoint(0, m_connect->height())));
    if (!chosen)
        return;
    const QString platform = chosen->data().toString();
    if (platform == QLatin1String(OnlineAccount::kFics)) {
        connectFics();
        return;
    }
    if (platform != QLatin1String(OnlineAccount::kLichess))
        return;
    // The browser shows lichess's own sign-in page; the token comes back here.
    m_signIn = new LichessSignIn(this);
    connect(m_signIn, &LichessSignIn::openBrowser, this, [](const QUrl &url) { QDesktopServices::openUrl(url); });
    connect(m_signIn, &LichessSignIn::finished, this,
            [this, platform](const QString &token, const QString &username, const QString &error) {
                signedIn(platform, token, username, error);
            });
    m_signInStatus->setText(tr("Sign in to %1 in your browser to connect it…").arg(OnlineAccounts::platformName(platform)));
    m_signInStatus->show();
    m_signIn->start({QStringLiteral("board:play")}); // Playing needs this scope; reading games does not.
    updateButtons();
}

void PlayOnlineDialog::signedIn(const QString &platform, const QString &token, const QString &username, const QString &error)
{
    if (m_signIn) { // freechess.org asks for nothing in a browser.
        m_signIn->deleteLater();
        m_signIn = nullptr;
    }
    m_signInStatus->show();
    if (!error.isEmpty()) {
        m_signInStatus->setText(tr("Could not connect: %1").arg(error));
        updateButtons();
        return;
    }
    OnlineAccount account;
    account.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    account.platform = platform;
    account.username = username;
    // The same account signed in again keeps one entry; its old token goes.
    for (const OnlineAccount &existing : m_accounts.accounts()) {
        if (existing.platform == platform && existing.username.compare(username, Qt::CaseInsensitive) == 0)
            SourceCredentials::remove(existing.id);
    }
    m_accounts.add(account);
    SourceCredentials::setToken(account.id, token);
    QSettings settings;
    m_accounts.save(settings);
    m_signInStatus->setText(tr("Connected to %1 as %2.").arg(OnlineAccounts::platformName(platform), username));
    rebuildAccounts();
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == account.id)
            m_list->setCurrentRow(i);
    }
    updateButtons();
}

void PlayOnlineDialog::connectFics()
{
    // FICS has no sign-in page: a registered player's name and password, or
    // a guest, whose name the server gives at every session.
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Connect freechess.org"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(tr("The Free Internet Chess Server. Play as a guest, unrated games under a name the "
                                "server gives, or as a player registered at freechess.org."));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *guest = new QCheckBox(tr("Play as a guest"));
    guest->setChecked(true);
    layout->addWidget(guest);
    auto *form = new QFormLayout;
    auto *name = new QLineEdit;
    auto *password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Name:"), name);
    form->addRow(tr("Password:"), password);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    const auto fill = [&] {
        name->setEnabled(!guest->isChecked());
        password->setEnabled(!guest->isChecked());
        buttons->button(QDialogButtonBox::Ok)->setEnabled(guest->isChecked()
                                                          || (!name->text().trimmed().isEmpty() && !password->text().isEmpty()));
    };
    connect(guest, &QCheckBox::toggled, &dialog, fill);
    connect(name, &QLineEdit::textChanged, &dialog, fill);
    connect(password, &QLineEdit::textChanged, &dialog, fill);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    fill();
    if (dialog.exec() != QDialog::Accepted)
        return;
    // The password is kept where tokens are; a guest has none.
    if (guest->isChecked())
        signedIn(QLatin1String(OnlineAccount::kFics), QString(), QStringLiteral("guest"), QString());
    else
        signedIn(QLatin1String(OnlineAccount::kFics), password->text(), name->text().trimmed(), QString());
}

void PlayOnlineDialog::removeAccount()
{
    const OnlineAccount chosen = account();
    if (chosen.id.isEmpty())
        return;
    SourceCredentials::remove(chosen.id);
    m_accounts.remove(chosen.id);
    QSettings settings;
    m_accounts.save(settings);
    rebuildAccounts();
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    updateButtons();
}

