#include "ExtensionsDialog.h"

#include "app/extensions/ExtensionProvider.h"
#include "app/sources/SourceFetch.h"
#include "widgets/PaddedHeaderView.h"
#include "widgets/PaddedItemDelegate.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QStyledItemDelegate>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int kIndexRole = Qt::UserRole;
constexpr int kHostRole = Qt::UserRole + 1;
constexpr int kLogo = 36;
constexpr int kPadding = 10;

/// A provider in the list: its logo, its name in bold, its site under it.
class ProviderDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        QFont bold = option.font;
        bold.setBold(true);
        const int text = QFontMetrics(bold).height() + QFontMetrics(option.font).height() + 2;
        return {kLogo + 2 * kPadding, qMax(kLogo, text) + 2 * kPadding};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem background = option;
        initStyleOption(&background, index);
        background.text.clear();
        background.icon = QIcon();
        const QWidget *widget = option.widget;
        (widget ? widget->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &background, painter, widget);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        const QRect logo(option.rect.left() + kPadding, option.rect.center().y() - kLogo / 2, kLogo, kLogo);
        const QPixmap pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
        QPainterPath round;
        round.addRoundedRect(QRectF(logo), 7, 7);
        if (!pixmap.isNull()) {
            painter->setClipPath(round);
            painter->drawPixmap(logo, pixmap);
            painter->setClipping(false);
        } else {
            // Until the logo arrives: its initial on a tile.
            painter->fillPath(round, option.palette.color(QPalette::Mid));
            QFont initial = option.font;
            initial.setBold(true);
            initial.setPixelSize(kLogo / 2);
            painter->setFont(initial);
            painter->setPen(option.palette.color(QPalette::Light));
            painter->drawText(logo, Qt::AlignCenter, index.data(Qt::DisplayRole).toString().left(1));
        }
        const bool selected = option.state & QStyle::State_Selected;
        const QColor text = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        QColor soft = text;
        soft.setAlphaF(0.65);
        QFont bold = option.font;
        bold.setBold(true);
        const int left = logo.right() + kPadding;
        const int height = QFontMetrics(bold).height() + QFontMetrics(option.font).height() + 2;
        const int top = option.rect.center().y() - height / 2;
        const QRect textArea(left, top, option.rect.right() - kPadding - left, height);
        painter->setFont(bold);
        painter->setPen(text);
        painter->drawText(textArea, Qt::AlignLeft | Qt::AlignTop,
                          QFontMetrics(bold).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textArea.width()));
        painter->setFont(option.font);
        painter->setPen(soft);
        painter->drawText(textArea, Qt::AlignLeft | Qt::AlignBottom,
                          QFontMetrics(option.font).elidedText(index.data(kHostRole).toString(), Qt::ElideRight, textArea.width()));
        painter->restore();
    }
};

QString kindName(Extension::Kind kind)
{
    switch (kind) {
    case Extension::Kind::Engine: return ExtensionsDialog::tr("Engine");
    case Extension::Kind::Database: return ExtensionsDialog::tr("Database");
    }
    return {};
}

QString sizeText(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1);
}

QWidget *column(const QString &title, QWidget *parent, QVBoxLayout **layout)
{
    auto *widget = new QWidget(parent);
    *layout = new QVBoxLayout(widget);
    (*layout)->setContentsMargins(0, 0, 0, 0);
    (*layout)->setSpacing(8);
    auto *label = new QLabel(title, widget);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    (*layout)->addWidget(label);
    return widget;
}

} // namespace

ExtensionsDialog::ExtensionsDialog(std::function<QString(const InstalledExtension &)> addEngine,
                                   std::function<void(const QString &)> removeEngine, QWidget *parent)
    : QDialog(parent)
    , m_addEngine(std::move(addEngine))
    , m_removeEngine(std::move(removeEngine))
    , m_installer(new ExtensionInstaller(this))
    , m_network(new QNetworkAccessManager(this))
{
    setWindowTitle(tr("Manage Extensions"));
    m_providers << new EnCroissantProvider(this);
    QSettings settings;
    m_installed = InstalledExtension::load(settings);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(12);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(16); // Room between the columns.

    // The providers.
    QVBoxLayout *left = nullptr;
    splitter->addWidget(column(tr("Providers"), splitter, &left));
    m_providerList = new QListWidget;
    m_providerList->setItemDelegate(new ProviderDelegate(m_providerList));
    m_providerList->setSpacing(2);
    // The rows take the list's width: names and sites are cut with "…", never scrolled.
    m_providerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_providerList->setResizeMode(QListView::Adjust);
    for (ExtensionProvider *provider : std::as_const(m_providers)) {
        auto *item = new QListWidgetItem(provider->name(), m_providerList);
        item->setData(kHostRole, QUrl(provider->homepage()).host());
        item->setToolTip(provider->description());
        loadLogo(provider, item);
    }
    left->addWidget(m_providerList, 1);
    m_providerText = new QLabel;
    m_providerText->setWordWrap(true);
    m_providerText->setOpenExternalLinks(true);
    left->addWidget(m_providerText);

    // What the provider offers.
    QVBoxLayout *middle = nullptr;
    splitter->addWidget(column(tr("Extensions"), splitter, &middle));
    auto *filters = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_kind = new QComboBox;
    m_kind->addItem(tr("All"), -1);
    m_kind->addItem(tr("Engines"), int(Extension::Kind::Engine));
    m_kind->addItem(tr("Databases"), int(Extension::Kind::Database));
    filters->addWidget(m_search, 1);
    filters->addWidget(m_kind);
    middle->addLayout(filters);
    m_list = new QTreeWidget;
    PaddedHeaderView::install(m_list);
    m_list->setItemDelegate(new PaddedItemDelegate(CellPadding::vertical, CellPadding::horizontal, m_list));
    m_list->setAlternatingRowColors(true);
    m_list->setHeaderLabels({tr("Name"), tr("Version"), tr("Kind"), tr("Installed")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column)
        m_list->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    middle->addWidget(m_list, 1);
    m_listStatus = new QLabel;
    m_listStatus->setWordWrap(true);
    middle->addWidget(m_listStatus);

    // The extension chosen.
    QVBoxLayout *right = nullptr;
    splitter->addWidget(column(tr("Details"), splitter, &right));
    right->setSpacing(10);
    m_title = new QLabel;
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);
    right->addWidget(m_title);
    m_facts = new QLabel;
    m_facts->setWordWrap(true);
    m_facts->setTextFormat(Qt::RichText);
    m_facts->setOpenExternalLinks(true);
    right->addWidget(m_facts);
    m_note = new QLabel;
    m_note->setWordWrap(true);
    QFont noteFont = m_note->font();
    noteFont.setItalic(true);
    m_note->setFont(noteFont);
    right->addWidget(m_note);
    // The buttons right under what the extension is; how the download goes under them.
    auto *buttons = new QHBoxLayout;
    m_install = new QPushButton(tr("Install"));
    m_remove = new QPushButton(tr("Remove"));
    buttons->addWidget(m_install);
    buttons->addWidget(m_remove);
    buttons->addStretch(1);
    right->addSpacing(4);
    right->addLayout(buttons);
    m_progress = new QProgressBar;
    m_progress->setTextVisible(false);
    m_progress->hide();
    right->addWidget(m_progress);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    right->addWidget(m_status);
    right->addStretch(1);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 2);
    splitter->setSizes({220, 440, 300});
    layout->addWidget(splitter, 1);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &ExtensionsDialog::reject);
    layout->addWidget(close);

    connect(m_providerList, &QListWidget::currentRowChanged, this, &ExtensionsDialog::providerChosen);
    connect(m_search, &QLineEdit::textChanged, this, &ExtensionsDialog::fillList);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &ExtensionsDialog::fillList);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &ExtensionsDialog::showExtension);
    connect(m_install, &QPushButton::clicked, this, &ExtensionsDialog::install);
    connect(m_remove, &QPushButton::clicked, this, &ExtensionsDialog::remove);
    for (ExtensionProvider *each : std::as_const(m_providers)) {
        connect(each, &ExtensionProvider::ready, this, [this, each] {
            if (each == provider())
                fillList();
        });
        connect(each, &ExtensionProvider::failed, this, [this, each](const QString &message) {
            if (each == provider())
                m_listStatus->setText(tr("The catalog could not be read: %1").arg(message));
        });
    }
    connect(m_installer, &ExtensionInstaller::progress, this, [this](qint64 received, qint64 total) {
        m_progress->setRange(0, total > 0 ? 1000 : 0);
        if (total > 0)
            m_progress->setValue(int(1000 * received / total));
        m_status->setText(total > 0 ? tr("Downloading… %1 of %2").arg(sizeText(received), sizeText(total))
                                    : tr("Downloading… %1").arg(sizeText(received)));
    });
    connect(m_installer, &ExtensionInstaller::installed, this, [this](InstalledExtension done) {
        m_progress->hide();
        if (!done.executable.isEmpty() && m_addEngine)
            done.engineId = m_addEngine(done);
        m_installed.removeIf([&done](const InstalledExtension &old) { return old.provider == done.provider && old.id == done.id; });
        m_installed << done;
        saveInstalled();
        m_status->setText(done.executable.isEmpty() ? tr("Installed.")
                                                    : tr("Installed: %1 is in Engine ▸ Switch Engine.").arg(done.name));
        fillList();
    });
    connect(m_installer, &ExtensionInstaller::failed, this, [this](const QString &message) {
        m_progress->hide();
        m_status->setText(tr("Could not install: %1").arg(message));
        showExtension();
    });

    resize(1040, 600);
    m_providerList->setCurrentRow(0);
}

ExtensionsDialog::~ExtensionsDialog() = default;

void ExtensionsDialog::loadLogo(ExtensionProvider *provider, QListWidgetItem *item)
{
    const QString url = provider->logoUrl();
    if (url.isEmpty())
        return;
    // Kept in the cache, read again from the provider now and then.
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                          + QStringLiteral("/extensions/") + provider->id() + QStringLiteral(".png");
    const auto show = [this, item](const QPixmap &logo) {
        const qreal ratio = devicePixelRatioF();
        QPixmap scaled = logo.scaled(QSize(kLogo, kLogo) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(ratio);
        item->setData(Qt::DecorationRole, scaled);
    };
    QPixmap cached(cache);
    if (!cached.isNull())
        show(cached);
    if (!cached.isNull() && QFileInfo(cache).lastModified().daysTo(QDateTime::currentDateTime()) < 30)
        return;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, SourceFetch::userAgent());
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, cache, show] {
        reply->deleteLater();
        const QByteArray bytes = reply->readAll();
        QPixmap logo;
        if (reply->error() != QNetworkReply::NoError || !logo.loadFromData(bytes))
            return;
        QDir().mkpath(QFileInfo(cache).absolutePath());
        QFile file(cache);
        if (file.open(QIODevice::WriteOnly))
            file.write(bytes);
        show(logo);
    });
}

void ExtensionsDialog::showOnly(const QString &provider, Extension::Kind kind)
{
    for (int i = 0; i < m_providers.size(); ++i) {
        if (m_providers.at(i)->id() == provider)
            m_providerList->setCurrentRow(i);
    }
    m_search->clear();
    m_kind->setCurrentIndex(qMax(0, m_kind->findData(int(kind))));
}

bool ExtensionsDialog::choose(const QString &name, bool install)
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        if (m_list->topLevelItem(i)->text(0).startsWith(name, Qt::CaseInsensitive)) {
            m_list->setCurrentItem(m_list->topLevelItem(i));
            if (install && m_install->isEnabled())
                this->install();
            return true;
        }
    }
    return false;
}

void ExtensionsDialog::reject()
{
    if (m_installer->isBusy())
        m_installer->cancel(); // Closing stops a download; nothing half is kept.
    QDialog::reject();
}

ExtensionProvider *ExtensionsDialog::provider() const
{
    const int row = m_providerList->currentRow();
    return row >= 0 && row < m_providers.size() ? m_providers.at(row) : nullptr;
}

const Extension *ExtensionsDialog::extension() const
{
    const ExtensionProvider *chosen = provider();
    const QTreeWidgetItem *item = m_list->currentItem();
    if (!chosen || !item)
        return nullptr;
    const int index = item->data(0, kIndexRole).toInt();
    return index >= 0 && index < chosen->extensions().size() ? &chosen->extensions().at(index) : nullptr;
}

const InstalledExtension *ExtensionsDialog::installedOf(const QString &provider, const QString &id) const
{
    for (const InstalledExtension &installed : m_installed) {
        if (installed.provider == provider && installed.id == id)
            return &installed;
    }
    return nullptr;
}

void ExtensionsDialog::providerChosen()
{
    ExtensionProvider *chosen = provider();
    if (!chosen)
        return;
    m_providerText->setText(QStringLiteral("%1<br><a href=\"%2\">%3</a>")
                                .arg(chosen->description().toHtmlEscaped(), chosen->homepage(),
                                     QUrl(chosen->homepage()).host()));
    if (chosen->extensions().isEmpty()) {
        m_listStatus->setText(tr("Reading the catalog…"));
        chosen->fetch();
    }
    fillList();
}

void ExtensionsDialog::fillList()
{
    const ExtensionProvider *chosen = provider();
    const QString current = extension() ? extension()->id : QString();
    m_list->clear();
    if (!chosen)
        return;
    const QString search = m_search->text().trimmed();
    const int kind = m_kind->currentData().toInt();
    int shown = 0;
    for (int i = 0; i < chosen->extensions().size(); ++i) {
        const Extension &each = chosen->extensions().at(i);
        if (kind >= 0 && int(each.kind) != kind)
            continue;
        if (!search.isEmpty() && !each.name.contains(search, Qt::CaseInsensitive)
            && !each.description.contains(search, Qt::CaseInsensitive))
            continue;
        const InstalledExtension *installed = installedOf(chosen->id(), each.id);
        auto *item = new QTreeWidgetItem(m_list, {each.name, each.version, kindName(each.kind),
                                                  installed ? installed->version : QString()});
        item->setData(0, kIndexRole, i);
        if (!each.installable())
            item->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
        if (each.id == current)
            m_list->setCurrentItem(item);
        ++shown;
    }
    if (!chosen->extensions().isEmpty())
        m_listStatus->setText(tr("%n extension(s)", nullptr, shown));
    if (!m_list->currentItem() && m_list->topLevelItemCount() > 0)
        m_list->setCurrentItem(m_list->topLevelItem(0));
    showExtension();
}

void ExtensionsDialog::showExtension()
{
    const Extension *chosen = extension();
    const InstalledExtension *installed = chosen ? installedOf(provider()->id(), chosen->id) : nullptr;
    const bool busy = m_installer->isBusy();
    if (!chosen) {
        m_title->clear();
        m_facts->clear();
        m_note->clear();
        m_install->setEnabled(false);
        m_remove->setEnabled(false);
        return;
    }
    m_title->setText(chosen->version.isEmpty() ? chosen->name : chosen->name + QLatin1Char(' ') + chosen->version);
    QStringList facts{kindName(chosen->kind)};
    if (chosen->elo > 0)
        facts << tr("Elo about %1").arg(chosen->elo);
    if (chosen->count > 0)
        facts << (chosen->puzzles ? tr("%1 puzzles") : tr("%1 games")).arg(QLocale().toString(chosen->count));
    if (chosen->downloadSize > 0)
        facts << tr("Download: %1").arg(sizeText(chosen->downloadSize));
    if (!chosen->downloadUrl.isEmpty())
        facts << tr("From <a href=\"%1\">%2</a>").arg(chosen->downloadUrl.toHtmlEscaped(), QUrl(chosen->downloadUrl).host());
    if (installed)
        facts << tr("Installed: version %1").arg(installed->version.toHtmlEscaped());
    if (!chosen->description.isEmpty())
        facts << chosen->description.toHtmlEscaped();
    m_facts->setText(facts.join(QStringLiteral("<br>")));
    m_note->setText(chosen->unavailable);
    m_install->setText(installed && installed->version != chosen->version ? tr("Update") : tr("Install"));
    m_install->setEnabled(!busy && chosen->installable() && (!installed || installed->version != chosen->version));
    m_remove->setEnabled(!busy && installed);
}

void ExtensionsDialog::install()
{
    const Extension *chosen = extension();
    if (!chosen || m_installer->isBusy())
        return;
    // An update replaces the engine in Manage Engines too.
    if (const InstalledExtension *old = installedOf(provider()->id(), chosen->id); old && !old->engineId.isEmpty() && m_removeEngine)
        m_removeEngine(old->engineId);
    m_progress->setRange(0, 0);
    m_progress->show();
    m_status->setText(tr("Downloading…"));
    m_installer->install(provider()->id(), *chosen);
    showExtension();
}

void ExtensionsDialog::remove()
{
    const Extension *chosen = extension();
    const InstalledExtension *installed = chosen ? installedOf(provider()->id(), chosen->id) : nullptr;
    if (!installed)
        return;
    const InstalledExtension gone = *installed;
    if (!gone.engineId.isEmpty() && m_removeEngine)
        m_removeEngine(gone.engineId);
    QString error;
    if (!ExtensionInstaller::remove(gone, &error))
        m_status->setText(tr("Could not remove: %1").arg(error));
    else
        m_status->setText(tr("Removed."));
    m_installed.removeIf([&gone](const InstalledExtension &each) { return each.provider == gone.provider && each.id == gone.id; });
    saveInstalled();
    fillList();
}

void ExtensionsDialog::saveInstalled()
{
    QSettings settings;
    InstalledExtension::save(settings, m_installed);
}
