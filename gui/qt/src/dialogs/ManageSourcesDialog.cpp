#include "ManageSourcesDialog.h"

#include "SourceSettingsWidget.h"
#include "app/GameDatabase.h"
#include "app/sources/SourceCatalog.h"
#include "app/sources/SourceCredentials.h"
#include "app/sources/SourceSync.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

std::optional<GameSource> findSource(GameDatabase *database, qint64 id)
{
    for (const GameSource &source : database->sources()) {
        if (source.id == id)
            return source;
    }
    return std::nullopt;
}

} // namespace

ManageSourcesDialog::ManageSourcesDialog(GameDatabase *database, SourceSync *sync, QWidget *parent)
    : QDialog(parent)
    , m_database(database)
    , m_sync(sync)
    , m_list(new QTreeWidget)
    , m_syncButton(new QPushButton(tr("S&ync Now")))
    , m_editButton(new QPushButton(tr("&Edit…")))
    , m_signInButton(new QPushButton(tr("Sign &In Again…")))
    , m_removeButton(new QPushButton(tr("&Remove…")))
{
    setWindowTitle(tr("Sources of “%1”").arg(database->name()));
    resize(760, 360);

    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->setHeaderLabels({tr("Source"), tr("Games"), tr("Last Sync"), tr("Status")});
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setStretchLastSection(true);
    m_list->setAccessibleName(tr("Sources"));

    auto *connectButton = new QPushButton(tr("&Connect Source…"));
    auto *buttons = new QVBoxLayout;
    for (QPushButton *button : {connectButton, m_syncButton, m_editButton, m_signInButton, m_removeButton})
        buttons->addWidget(button);
    buttons->addStretch();

    auto *content = new QHBoxLayout;
    content->addWidget(m_list, 1);
    content->addLayout(buttons);

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(content);
    layout->addWidget(close);

    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &ManageSourcesDialog::updateButtons);
    connect(m_list, &QTreeWidget::itemActivated, this, &ManageSourcesDialog::editSource);
    connect(connectButton, &QPushButton::clicked, this, &ManageSourcesDialog::connectRequested);
    connect(m_syncButton, &QPushButton::clicked, this, [this] { m_sync->syncSource(selectedSourceId()); reload(); });
    connect(m_editButton, &QPushButton::clicked, this, &ManageSourcesDialog::editSource);
    connect(m_signInButton, &QPushButton::clicked, this, &ManageSourcesDialog::signInAgain);
    connect(m_removeButton, &QPushButton::clicked, this, &ManageSourcesDialog::removeSource);
    connect(m_sync, &SourceSync::sourcesChanged, this, &ManageSourcesDialog::reload);
    connect(m_sync, &SourceSync::gamesImported, this, &ManageSourcesDialog::reload);
    connect(m_sync, &SourceSync::activityChanged, this, &ManageSourcesDialog::reload);
    reload();
}

void ManageSourcesDialog::reload()
{
    const qint64 selected = selectedSourceId();
    m_list->clear();
    const QLocale locale;
    for (const GameSource &source : m_database->sources()) {
        QString status;
        if (m_sync->currentSource() == source.id)
            status = tr("Syncing…");
        else if (!source.lastError.isEmpty())
            status = source.lastError;
        else if (source.lastSyncAt.isValid())
            status = tr("Up to date");
        else
            status = tr("Waiting for the first sync");
        const std::optional<SourceKind> kind = SourceCatalog::kind(source.kind);
        if (kind && kind->needsSignIn && SourceCredentials::token(source.uuid).isEmpty())
            status = tr("Not signed in");

        auto *item = new QTreeWidgetItem(m_list);
        item->setText(0, SourceCatalog::displayName(source));
        item->setText(1, locale.toString(source.importedGames));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setText(2, source.lastSyncAt.isValid()
                             ? locale.toString(source.lastSyncAt.toLocalTime(), QLocale::ShortFormat)
                             : tr("Never"));
        item->setText(3, status);
        item->setToolTip(3, status);
        item->setData(0, Qt::UserRole, source.id);
        if (source.id == selected)
            m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem() && m_list->topLevelItemCount() > 0)
        m_list->setCurrentItem(m_list->topLevelItem(0));
    updateButtons();
}

qint64 ManageSourcesDialog::selectedSourceId() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    return item ? item->data(0, Qt::UserRole).toLongLong() : 0;
}

void ManageSourcesDialog::updateButtons()
{
    const std::optional<GameSource> source = findSource(m_database, selectedSourceId());
    const std::optional<SourceKind> kind = source ? SourceCatalog::kind(source->kind) : std::nullopt;
    m_syncButton->setEnabled(source.has_value());
    m_editButton->setEnabled(source.has_value());
    m_removeButton->setEnabled(source.has_value());
    m_signInButton->setEnabled(kind && kind->needsSignIn);
    m_signInButton->setToolTip(kind && !kind->needsSignIn ? tr("%1 does not need signing in.").arg(kind->name)
                                                          : QString());
}

void ManageSourcesDialog::editSource()
{
    std::optional<GameSource> source = findSource(m_database, selectedSourceId());
    const std::optional<SourceKind> kind = source ? SourceCatalog::kind(source->kind) : std::nullopt;
    if (!kind)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(SourceCatalog::displayName(*source));
    auto *settings = new SourceSettingsWidget(*kind, source->uuid);
    settings->setSource(*source);
    auto *error = new QLabel;
    error->setWordWrap(true);
    error->hide();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(settings);
    layout->addWidget(error);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QString message;
        if (!settings->validate(&message)) {
            error->setText(message);
            error->show();
            return;
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted)
        return;

    const GameSource before = *source;
    settings->applyTo(*source);
    if (source->account != before.account || source->settings != before.settings)
        source->state = {}; // Import again with the new choice; known games are skipped.
    QString message;
    m_sync->cancelSource(source->id);
    if (!m_database->updateSource(*source, &message)) {
        QMessageBox::warning(this, tr("Edit Source"), tr("Could not save the source: %1").arg(message));
        return;
    }
    m_sync->syncSource(source->id);
    reload();
}

void ManageSourcesDialog::signInAgain()
{
    const std::optional<GameSource> source = findSource(m_database, selectedSourceId());
    const std::optional<SourceKind> kind = source ? SourceCatalog::kind(source->kind) : std::nullopt;
    if (!kind || !kind->needsSignIn)
        return;
    if (SourceSettingsWidget::signIn(*kind, source->uuid, this))
        m_sync->syncSource(source->id);
    reload();
}

void ManageSourcesDialog::removeSource()
{
    const std::optional<GameSource> source = findSource(m_database, selectedSourceId());
    if (!source)
        return;
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Remove Source"),
        tr("Remove “%1” from this database?\n\nThe %n game(s) already imported stay in the database.", nullptr,
           int(source->importedGames)).arg(SourceCatalog::displayName(*source)),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    m_sync->cancelSource(source->id);
    QString message;
    if (!m_database->removeSource(source->id, &message)) {
        QMessageBox::warning(this, tr("Remove Source"), tr("Could not remove the source: %1").arg(message));
        return;
    }
    SourceCredentials::remove(source->uuid);
    reload();
}
