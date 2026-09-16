#include "SyncDialog.h"

#include "app/UserFolders.h"
#include "app/sync/FolderSync.h"
#include "app/sync/RemoteStore.h"
#include "app/sync/SyncManifest.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

SyncDialog::SyncDialog(const SyncSettings &settings, FolderSync *sync, QWidget *parent)
    : QDialog(parent)
    , m_sync(sync)
    , m_service(new QComboBox)
    , m_pages(new QStackedWidget)
    , m_host(new QLineEdit(settings.host))
    , m_port(new QSpinBox)
    , m_tls(new QCheckBox(tr("Use TLS (FTPS)")))
    , m_folder(new QLineEdit(settings.folder))
    , m_url(new QLineEdit(settings.url.toString()))
    , m_user(new QLineEdit(settings.user))
    , m_password(new QLineEdit(settings.password))
    , m_status(new QLabel)
    , m_testButton(new QPushButton(tr("&Test Connection")))
    , m_syncButton(new QPushButton(tr("S&ync Now")))
{
    setWindowTitle(tr("Sync"));
    setModal(true);
    setMinimumWidth(520);

    auto *intro = new QLabel(tr("Keep the databases and projects of <b>%1</b> the same on every computer: "
                                "set each one up with the same folder on a server.")
                                 .arg(QDir::toNativeSeparators(UserFolders::pragmaDir()).toHtmlEscaped()));
    intro->setWordWrap(true);

    m_service->addItem(tr("Off"), int(SyncSettings::Service::None));
    m_service->addItem(tr("FTP"), int(SyncSettings::Service::Ftp));
    m_service->addItem(tr("WebDAV"), int(SyncSettings::Service::WebDav));
    m_service->setCurrentIndex(m_service->findData(int(settings.service)));

    // FTP
    auto *ftp = new QWidget;
    auto *ftpForm = new QFormLayout(ftp);
    ftpForm->setContentsMargins(0, 0, 0, 0);
    m_host->setPlaceholderText(QStringLiteral("ftp.example.com"));
    m_port->setRange(1, 65535);
    m_port->setValue(settings.port);
    m_tls->setChecked(settings.tls);
    auto *serverRow = new QHBoxLayout;
    serverRow->addWidget(m_host, 1);
    serverRow->addWidget(new QLabel(tr("Port:")));
    serverRow->addWidget(m_port);
    auto *serverLabel = new QLabel(tr("&Server:"));
    serverLabel->setBuddy(m_host);
    ftpForm->addRow(serverLabel, serverRow);
    ftpForm->addRow(QString(), m_tls);
    m_folder->setPlaceholderText(tr("Folder on the server, e.g. Chess/Pragma"));
    ftpForm->addRow(tr("&Folder:"), m_folder);

    // WebDAV
    auto *webdav = new QWidget;
    auto *webdavForm = new QFormLayout(webdav);
    webdavForm->setContentsMargins(0, 0, 0, 0);
    m_url->setPlaceholderText(QStringLiteral("https://cloud.example.com/remote.php/dav/files/me/Pragma"));
    webdavForm->addRow(tr("&Address:"), m_url);

    m_pages->addWidget(new QWidget);
    m_pages->addWidget(ftp);
    m_pages->addWidget(webdav);

    auto *form = new QFormLayout;
    form->addRow(tr("S&ervice:"), m_service);
    form->addRow(m_pages);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("&User:"), m_user);
    form->addRow(tr("&Password:"), m_password);

    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *actions = new QHBoxLayout;
    actions->addWidget(m_testButton);
    actions->addWidget(m_syncButton);
    actions->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addSpacing(8);
    layout->addLayout(form);
    layout->addLayout(actions);
    layout->addWidget(m_status);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_service, &QComboBox::currentIndexChanged, this, &SyncDialog::updateFields);
    connect(m_testButton, &QPushButton::clicked, this, &SyncDialog::testConnection);
    connect(m_syncButton, &QPushButton::clicked, this, [this] { Q_EMIT syncRequested(this->settings()); });
    connect(m_sync, &FolderSync::started, this, &SyncDialog::updateStatus);
    connect(m_sync, &FolderSync::progress, m_status, &QLabel::setText);
    connect(m_sync, &FolderSync::finished, this, &SyncDialog::updateStatus);
    updateFields();
    updateStatus();
}

SyncSettings SyncDialog::settings() const
{
    SyncSettings result;
    result.service = SyncSettings::Service(m_service->currentData().toInt());
    result.host = m_host->text().trimmed();
    result.port = quint16(m_port->value());
    result.tls = m_tls->isChecked();
    result.folder = m_folder->text().trimmed();
    result.url = QUrl::fromUserInput(m_url->text().trimmed());
    result.user = m_user->text();
    result.password = m_password->text();
    return result;
}

void SyncDialog::updateFields()
{
    const auto service = SyncSettings::Service(m_service->currentData().toInt());
    m_pages->setCurrentIndex(m_service->currentIndex());
    const bool on = service != SyncSettings::Service::None;
    for (QWidget *widget : {static_cast<QWidget *>(m_user), static_cast<QWidget *>(m_password),
                            static_cast<QWidget *>(m_testButton), static_cast<QWidget *>(m_syncButton)})
        widget->setEnabled(on);
    updateStatus();
}

void SyncDialog::updateStatus()
{
    if (m_sync->isRunning()) {
        m_status->setText(tr("Syncing…"));
        return;
    }
    if (!m_sync->lastError().isEmpty())
        m_status->setText(tr("The last sync failed: %1").arg(m_sync->lastError()));
    else if (m_sync->lastSync().isValid())
        m_status->setText(tr("Last synced %1.").arg(QLocale().toString(m_sync->lastSync().toLocalTime(), QLocale::ShortFormat)));
    else
        m_status->setText(QString());
}

void SyncDialog::testConnection()
{
    const SyncSettings current = settings();
    if (!current.isConfigured()) {
        m_status->setText(tr("Enter the server first."));
        return;
    }
    RemoteStore *store = current.createStore(this);
    m_status->setText(tr("Connecting…"));
    m_testButton->setEnabled(false);
    QApplication::setOverrideCursor(Qt::BusyCursor);

    RemoteStore::Result result;
    QEventLoop loop;
    store->read(QLatin1String(SyncManifest::fileName), [&](const RemoteStore::Result &read) {
        result = read;
        loop.quit();
    });
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    QApplication::restoreOverrideCursor();
    m_testButton->setEnabled(true);
    store->abort();
    store->deleteLater();

    if (!result.ok) {
        m_status->setText(result.error.isEmpty() ? tr("No answer from the server.") : result.error);
        return;
    }
    if (result.notFound) {
        m_status->setText(tr("Connected. The folder is not synced yet: this computer will fill it."));
        return;
    }
    QString error;
    const std::optional<SyncManifest> manifest = SyncManifest::fromJson(result.data, &error);
    if (!manifest) {
        m_status->setText(error);
        return;
    }
    int files = 0;
    for (const SyncFileState &file : manifest->files)
        files += file.deleted ? 0 : 1;
    m_status->setText(tr("Connected. The folder holds %n file(s), last synced by %1 on %2.", nullptr, files)
                          .arg(manifest->updatedBy,
                               QLocale().toString(manifest->updatedAt.toLocalTime(), QLocale::ShortFormat)));
}
