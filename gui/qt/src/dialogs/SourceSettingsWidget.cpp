#include "SourceSettingsWidget.h"

#include "app/sources/LichessSignIn.h"
#include "app/sources/SourceCredentials.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateEdit>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QPushButton>
#include <QTimer>

SourceSettingsWidget::SourceSettingsWidget(const SourceKind &kind, const QString &sourceUuid, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
    , m_uuid(sourceUuid)
    , m_account(new QLineEdit)
    , m_limitSince(new QCheckBox(tr("Only games played since")))
    , m_since(new QDateEdit(QDate::currentDate().addYears(-1)))
    , m_ratedOnly(new QCheckBox(tr("Only rated games")))
{
    auto *form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);

    if (m_kind.needsSignIn) {
        m_signInStatus = new QLabel;
        m_signInButton = new QPushButton;
        auto *row = new QHBoxLayout;
        row->addWidget(m_signInStatus, 1);
        row->addWidget(m_signInButton);
        form->addRow(tr("Sign in:"), row);
        connect(m_signInButton, &QPushButton::clicked, this, [this] {
            if (const std::optional<QString> account = signIn(m_kind, m_uuid, this)) {
                if (m_account->text().trimmed().isEmpty())
                    m_account->setText(*account);
                updateSignInStatus();
                Q_EMIT changed();
            }
        });
        updateSignInStatus();
    }

    m_account->setPlaceholderText(tr("Username on %1").arg(m_kind.name));
    m_account->setClearButtonEnabled(true);
    form->addRow(tr("&Account:"), m_account);

    m_since->setCalendarPopup(true);
    m_since->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    m_since->setEnabled(false);
    auto *sinceRow = new QHBoxLayout;
    sinceRow->addWidget(m_limitSince);
    sinceRow->addWidget(m_since);
    sinceRow->addStretch();
    form->addRow(tr("Games:"), sinceRow);
    form->addRow(QString(), m_ratedOnly);

    auto *note = new QLabel(tr("New games are added while the database is open."));
    note->setWordWrap(true);
    note->setEnabled(false);
    form->addRow(QString(), note);

    connect(m_limitSince, &QCheckBox::toggled, m_since, &QWidget::setEnabled);
    connect(m_account, &QLineEdit::textChanged, this, &SourceSettingsWidget::changed);
}

void SourceSettingsWidget::setSource(const GameSource &source)
{
    m_account->setText(source.account);
    const QString since = source.settings.value(QLatin1String(SourceSettings::since)).toString();
    m_limitSince->setChecked(!since.isEmpty());
    if (!since.isEmpty())
        m_since->setDate(QDate::fromString(since, Qt::ISODate));
    m_ratedOnly->setChecked(source.settings.value(QLatin1String(SourceSettings::ratedOnly)).toBool());
}

void SourceSettingsWidget::applyTo(GameSource &source) const
{
    source.account = m_account->text().trimmed();
    source.settings.remove(QLatin1String(SourceSettings::since));
    if (m_limitSince->isChecked())
        source.settings.insert(QLatin1String(SourceSettings::since), m_since->date().toString(Qt::ISODate));
    source.settings.insert(QLatin1String(SourceSettings::ratedOnly), m_ratedOnly->isChecked());
}

bool SourceSettingsWidget::validate(QString *errorMessage)
{
    const QString account = m_account->text().trimmed();
    if (account.isEmpty()) {
        *errorMessage = tr("Enter the account whose games to import.");
        return false;
    }
    if (m_kind.needsSignIn && SourceCredentials::token(m_uuid).isEmpty()) {
        *errorMessage = tr("Sign in to %1 first.").arg(m_kind.name);
        return false;
    }

    QNetworkAccessManager network;
    QNetworkRequest request = SourceCatalog::accountRequest(m_kind.id, account);
    request.setTransferTimeout(15000);
    QApplication::setOverrideCursor(Qt::BusyCursor);
    QNetworkReply *reply = network.get(request);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    QApplication::restoreOverrideCursor();
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404) {
        *errorMessage = tr("There is no account “%1” on %2.").arg(account, m_kind.name);
        return false;
    }
    if (reply->error() != QNetworkReply::NoError) {
        *errorMessage = tr("Could not check the account on %1: %2").arg(m_kind.name, reply->errorString());
        return false;
    }
    return true;
}

std::optional<QString> SourceSettingsWidget::signIn(const SourceKind &kind, const QString &sourceUuid, QWidget *parent)
{
    if (kind.id != QLatin1String("lichess"))
        return std::nullopt;

    LichessSignIn flow;
    QProgressDialog progress(tr("Sign in to %1 in the browser that just opened.").arg(kind.name), tr("Cancel"), 0, 0,
                             parent);
    progress.setWindowTitle(tr("Sign In"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    QString token;
    QString username;
    QString error;
    bool done = false;
    QEventLoop loop;
    connect(&flow, &LichessSignIn::openBrowser, parent, [](const QUrl &url) { QDesktopServices::openUrl(url); });
    connect(&flow, &LichessSignIn::finished, &loop,
            [&](const QString &newToken, const QString &account, const QString &message) {
                token = newToken;
                username = account;
                error = message;
                done = true;
                loop.quit();
            });
    connect(&progress, &QProgressDialog::canceled, &loop, &QEventLoop::quit);
    QTimer::singleShot(0, &flow, &LichessSignIn::start);
    progress.show();
    loop.exec();
    progress.close();

    if (!done) {
        flow.cancel();
        return std::nullopt;
    }
    if (token.isEmpty()) {
        QMessageBox::warning(parent, tr("Sign In"), tr("Could not sign in to %1: %2").arg(kind.name, error));
        return std::nullopt;
    }
    SourceCredentials::setToken(sourceUuid, token);
    return username;
}

void SourceSettingsWidget::updateSignInStatus()
{
    const bool signedIn = !SourceCredentials::token(m_uuid).isEmpty();
    m_signInStatus->setText(signedIn ? tr("Signed in to %1").arg(m_kind.name) : tr("Not signed in"));
    m_signInButton->setText(signedIn ? tr("Sign In Again…") : tr("Sign In with %1…").arg(m_kind.name));
}
