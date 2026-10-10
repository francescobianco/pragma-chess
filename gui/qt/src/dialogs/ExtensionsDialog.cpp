#include "ExtensionsDialog.h"

#include "app/extensions/ExtensionProvider.h"

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

QString kindName(Extension::Kind kind)
{
    switch (kind) {
    case Extension::Kind::Engine: return ExtensionsDialog::tr("Engine");
    case Extension::Kind::Database: return ExtensionsDialog::tr("Database");
    case Extension::Kind::Puzzles: return ExtensionsDialog::tr("Puzzles");
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
{
    setWindowTitle(tr("Manage Extensions"));
    m_providers << new EnCroissantProvider(this);
    QSettings settings;
    m_installed = InstalledExtension::load(settings);

    auto *layout = new QVBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // The providers.
    QVBoxLayout *left = nullptr;
    splitter->addWidget(column(tr("Providers"), splitter, &left));
    m_providerList = new QListWidget;
    for (ExtensionProvider *provider : std::as_const(m_providers))
        m_providerList->addItem(provider->name());
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
    m_kind->addItem(tr("Puzzles"), int(Extension::Kind::Puzzles));
    filters->addWidget(m_search, 1);
    filters->addWidget(m_kind);
    middle->addLayout(filters);
    m_list = new QTreeWidget;
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
    right->addStretch(1);
    m_progress = new QProgressBar;
    m_progress->setTextVisible(false);
    m_progress->hide();
    right->addWidget(m_progress);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    right->addWidget(m_status);
    auto *buttons = new QHBoxLayout;
    m_install = new QPushButton(tr("Install"));
    m_remove = new QPushButton(tr("Remove"));
    buttons->addWidget(m_install);
    buttons->addWidget(m_remove);
    buttons->addStretch(1);
    right->addLayout(buttons);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 2);
    splitter->setSizes({180, 420, 300});
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

    resize(980, 560);
    m_providerList->setCurrentRow(0);
}

ExtensionsDialog::~ExtensionsDialog() = default;

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
        facts << (chosen->kind == Extension::Kind::Puzzles ? tr("%1 puzzles") : tr("%1 games")).arg(QLocale().toString(chosen->count));
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
