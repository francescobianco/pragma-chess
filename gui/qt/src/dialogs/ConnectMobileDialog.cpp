#include "ConnectMobileDialog.h"

#include "widgets/PaddedHeaderView.h"
#include "widgets/StoreBadge.h"

#include "app/phone/PhoneLink.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <qrcodegen.hpp>

namespace {

constexpr int kQuietZone = 4;
constexpr int kCodeSize = 240;

// Dark modules on white, whatever the theme: phone cameras need the contrast.
QPixmap qrPixmap(const QString &text, qreal devicePixelRatio)
{
    const qrcodegen::QrCode code =
        qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int modules = code.getSize() + 2 * kQuietZone;
    QImage image(modules, modules, QImage::Format_RGB32);
    image.fill(Qt::white);
    for (int y = 0; y < code.getSize(); ++y) {
        for (int x = 0; x < code.getSize(); ++x) {
            if (code.getModule(x, y))
                image.setPixel(x + kQuietZone, y + kQuietZone, qRgb(0, 0, 0));
        }
    }
    // Whole device pixels per module keep the modules even.
    const int scale = qMax(1, int(kCodeSize * devicePixelRatio) / modules);
    QPixmap pixmap = QPixmap::fromImage(image.scaled(modules * scale, modules * scale));
    pixmap.setDevicePixelRatio(devicePixelRatio);
    return pixmap;
}

QString when(const QDateTime &time)
{
    return time.isValid() ? QLocale().toString(time.toLocalTime(), QLocale::ShortFormat) : QString();
}

constexpr int kAppIconSize = 64;
// The APK of the latest release: the site's and the README's permanent link.
constexpr char kApkUrl[] = "https://github.com/francescobianco/pragma-chess/releases/latest/download/PragmaChess-android.apk";

} // namespace

ConnectMobileDialog::ConnectMobileDialog(PhoneLink *link, QWidget *parent)
    : QDialog(parent)
    , m_link(link)
    , m_code(new QLabel)
    , m_linkText(new QLineEdit)
    , m_devices(new QTreeWidget)
    , m_noDevices(new QLabel(tr("No device is connected yet.")))
    , m_status(new QLabel)
{
    setWindowTitle(tr("Connect Mobile App"));

    // The app as the phone shows it, to know it at a glance, and where to get it.
    auto *appIcon = new QLabel;
    const QPixmap rich(QStringLiteral(":/icons/pragma-chess-rich.png"));
    const qreal ratio = devicePixelRatioF();
    QPixmap icon = rich.scaled(QSize(kAppIconSize, kAppIconSize) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    icon.setDevicePixelRatio(ratio);
    appIcon->setPixmap(icon);
    appIcon->setFixedSize(kAppIconSize, kAppIconSize);
    auto *appTitle = new QLabel(tr("Pragma Chess for your phone"));
    QFont titleFont = appTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.25);
    appTitle->setFont(titleFont);
    auto *appText = new QLabel(tr("For Android phones and tablets; it is coming to the stores. Install it, then scan "
                                  "the code below with it."));
    appText->setWordWrap(true);
    auto *appWords = new QVBoxLayout;
    appWords->addWidget(appTitle);
    appWords->addWidget(appText);
    auto *appRow = new QHBoxLayout;
    appRow->addWidget(appIcon, 0, Qt::AlignTop);
    appRow->addSpacing(8);
    appRow->addLayout(appWords, 1);
    auto *badges = new QHBoxLayout;
    badges->setSpacing(8);
    badges->addWidget(new StoreBadge(tr("Download the APK for"), QStringLiteral("Android"), QUrl(QString::fromLatin1(kApkUrl))));
    for (const char *store : {"Google Play", "F-Droid", "App Store"})
        badges->addWidget(new StoreBadge(tr("Coming soon on"), QString::fromLatin1(store), QUrl()));

    auto *intro = new QLabel(tr("Scan this code with Pragma Chess on your phone or tablet. It copies your databases "
                                "to the device, and the games you play there come back to this computer."));
    intro->setWordWrap(true);

    m_code->setAlignment(Qt::AlignCenter);
    m_code->setMinimumSize(kCodeSize, kCodeSize);

    m_linkText->setReadOnly(true);
    m_linkText->setToolTip(tr("The same link as the code, to paste on the phone"));
    auto *copy = new QPushButton(tr("Copy"));
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_linkText->text()); });
    auto *linkRow = new QHBoxLayout;
    linkRow->addWidget(m_linkText, 1);
    linkRow->addWidget(copy);

    auto *devicesTitle = new QLabel(tr("Connected devices"));
    QFont bold = devicesTitle->font();
    bold.setBold(true);
    devicesTitle->setFont(bold);
    PaddedHeaderView::install(m_devices); // Room around titles and cells, as in the main window.
    m_devices->setColumnCount(4);
    m_devices->setHeaderLabels({tr("Name"), tr("Paired"), tr("Last Sync"), QString()});
    m_devices->setRootIsDecorated(false);
    m_devices->setSelectionMode(QAbstractItemView::NoSelection);
    m_devices->setFocusPolicy(Qt::NoFocus);
    m_devices->header()->setStretchLastSection(false);
    m_devices->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column)
        m_devices->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_noDevices->setEnabled(false);

    m_status->setEnabled(false); // Discreet: the disabled text colour.
    QFont small = m_status->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    m_status->setFont(small);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(appRow);
    layout->addSpacing(6);
    layout->addLayout(badges);
    layout->addSpacing(4);
    layout->addWidget(line);
    layout->addWidget(intro);
    layout->addWidget(m_code);
    layout->addLayout(linkRow);
    layout->addSpacing(8);
    layout->addWidget(devicesTitle);
    layout->addWidget(m_devices);
    layout->addWidget(m_noDevices);
    layout->addWidget(m_status);
    layout->addStretch(); // Room left over goes under the list, not between the parts.
    layout->addWidget(buttons);
    resize(560, 700);

    connect(m_link, &PhoneLink::pairingLinkChanged, this, &ConnectMobileDialog::updateCode);
    connect(m_link, &PhoneLink::devicesChanged, this, &ConnectMobileDialog::updateDevices);
    connect(m_link, &PhoneLink::statusChanged, this, &ConnectMobileDialog::updateStatus);
    connect(m_link, &PhoneLink::devicePaired, this, [this](const QString &name) {
        m_lastPaired = name;
        updateStatus();
    });

    m_link->beginPairing();
    updateCode();
    updateDevices();
    updateStatus();
}

ConnectMobileDialog::~ConnectMobileDialog()
{
    m_link->endPairing();
}

void ConnectMobileDialog::updateCode()
{
    const QString text = m_link->pairingLink().toString();
    m_linkText->setText(text);
    m_linkText->setCursorPosition(0);
    m_code->setPixmap(qrPixmap(text, devicePixelRatioF()));
}

void ConnectMobileDialog::updateDevices()
{
    m_devices->clear();
    const QList<PhoneLink::Device> devices = m_link->devices();
    for (const PhoneLink::Device &device : devices) {
        auto *item = new QTreeWidgetItem(m_devices, {device.name, when(device.pairedAt),
                                                     device.lastSyncAt.isValid() ? when(device.lastSyncAt)
                                                                                 : tr("Never")});
        auto *disconnect = new QPushButton(tr("Disconnect"));
        disconnect->setToolTip(tr("Forget this device: it can no longer copy the databases"));
        const QString key = device.key;
        connect(disconnect, &QPushButton::clicked, this, [this, key] {
            // After the click has been handled: this removes the button.
            QMetaObject::invokeMethod(this, [this, key] { m_link->removeDevice(key); }, Qt::QueuedConnection);
        });
        m_devices->setItemWidget(item, 3, disconnect);
    }
    m_devices->setVisible(!devices.isEmpty());
    m_noDevices->setVisible(devices.isEmpty());
}

void ConnectMobileDialog::updateStatus()
{
    QStringList parts;
    if (!m_lastPaired.isEmpty())
        parts << tr("%1 is connected.").arg(m_lastPaired);
    parts << tr("Relays: %1 of %2").arg(m_link->connectedRelays()).arg(m_link->relays().size());
    if (!m_link->activity().isEmpty())
        parts << m_link->activity();
    m_status->setText(parts.join(QStringLiteral(" · ")));
}
