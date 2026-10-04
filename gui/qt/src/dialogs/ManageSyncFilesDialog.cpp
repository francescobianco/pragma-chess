#include "ManageSyncFilesDialog.h"

#include "app/sync/FolderSync.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

enum Column { Name, Size, Modified, Device };

/// Sorts sizes and dates by value, not by their text.
class FileItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : Name;
        if (column == Size || column == Modified)
            return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
        return QTreeWidgetItem::operator<(other);
    }
};

} // namespace

ManageSyncFilesDialog::ManageSyncFilesDialog(FolderSync *sync, QWidget *parent)
    : QDialog(parent)
    , m_sync(sync)
    , m_files(new QTreeWidget)
    , m_status(new QLabel)
    , m_deleteButton(new QPushButton(tr("&Delete…")))
    , m_refreshButton(new QPushButton(tr("&Refresh")))
{
    setWindowTitle(tr("Manage Files"));
    setModal(true);
    resize(640, 420);

    auto *intro = new QLabel(tr("The files in the sync folder on the server. A file deleted here is deleted from "
                                "every synced device, this computer included."));
    intro->setWordWrap(true);

    m_files->setHeaderLabels({tr("File"), tr("Size"), tr("Modified"), tr("Device")});
    m_files->setRootIsDecorated(false);
    m_files->setUniformRowHeights(true);
    m_files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_files->setSortingEnabled(true);
    m_files->sortByColumn(Name, Qt::AscendingOrder);
    m_files->header()->setStretchLastSection(false);
    m_files->header()->setSectionResizeMode(Name, QHeaderView::Stretch);
    for (const int column : {Size, Modified, Device})
        m_files->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);

    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->addButton(m_deleteButton, QDialogButtonBox::ActionRole);
    buttons->addButton(m_refreshButton, QDialogButtonBox::ActionRole);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_files, 1);
    layout->addWidget(m_status);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_deleteButton, &QPushButton::clicked, this, &ManageSyncFilesDialog::deleteSelected);
    connect(m_refreshButton, &QPushButton::clicked, this, &ManageSyncFilesDialog::load);
    connect(m_files, &QTreeWidget::itemSelectionChanged, this, &ManageSyncFilesDialog::updateButtons);
    connect(m_sync, &FolderSync::progress, this, [this](const QString &text) {
        if (m_waiting)
            m_status->setText(text);
    });
    // A sync was running, or this dialog started one: list what it left.
    connect(m_sync, &FolderSync::finished, this, [this](const QString &error) {
        if (!m_waiting)
            return;
        m_waiting = false;
        if (!error.isEmpty()) {
            setBusy(false, tr("The sync failed: %1").arg(error));
            return;
        }
        load();
    });
    load();
}

void ManageSyncFilesDialog::setBusy(bool busy, const QString &status)
{
    m_busy = busy;
    m_status->setText(status);
    if (busy)
        setCursor(Qt::BusyCursor);
    else
        unsetCursor();
    updateButtons();
}

void ManageSyncFilesDialog::updateButtons()
{
    m_deleteButton->setEnabled(!m_busy && !m_files->selectedItems().isEmpty());
    m_refreshButton->setEnabled(!m_busy);
}

void ManageSyncFilesDialog::load()
{
    if (m_sync->isRunning()) {
        m_waiting = true;
        setBusy(true, tr("Waiting for the sync to finish…"));
        return;
    }
    setBusy(true, tr("Reading the folder on the server…"));
    const QPointer<ManageSyncFilesDialog> self(this);
    m_sync->listRemote([self](const QList<FolderSync::RemoteFile> &files, const QString &error) {
        if (!self)
            return;
        if (!error.isEmpty()) {
            self->setBusy(false, error);
            return;
        }
        self->m_files->setSortingEnabled(false);
        self->m_files->clear();
        const QLocale locale;
        for (const FolderSync::RemoteFile &file : files) {
            auto *item = new FileItem(self->m_files);
            item->setText(Name, QDir::toNativeSeparators(file.path));
            item->setData(Name, Qt::UserRole, file.path);
            item->setData(Size, Qt::UserRole, file.size);
            if (file.size >= 0) {
                item->setText(Size, locale.formattedDataSize(file.size));
                item->setTextAlignment(Size, Qt::AlignRight | Qt::AlignVCenter);
            }
            item->setData(Modified, Qt::UserRole, file.modified.isValid() ? file.modified.toMSecsSinceEpoch() : 0);
            if (file.modified.isValid())
                item->setText(Modified, locale.toString(file.modified.toLocalTime(), QLocale::ShortFormat));
            item->setText(Device, file.device);
        }
        self->m_files->setSortingEnabled(true);
        self->setBusy(false, files.isEmpty() ? tr("The folder on the server is empty.")
                                             : tr("%n file(s) on the server.", nullptr, int(files.size())));
    });
}

void ManageSyncFilesDialog::deleteSelected()
{
    QStringList paths;
    for (const QTreeWidgetItem *item : m_files->selectedItems())
        paths << item->data(Name, Qt::UserRole).toString();
    if (paths.isEmpty())
        return;
    paths.sort();

    const QString what = paths.size() == 1 ? QStringLiteral("“%1”").arg(QDir::toNativeSeparators(paths.constFirst()))
                                           : tr("%n file(s)", nullptr, int(paths.size()));
    QMessageBox warning(QMessageBox::Warning, tr("Delete Files Everywhere?"),
                        tr("Are you sure you want to delete %1 from every synced device?").arg(what),
                        QMessageBox::NoButton, this);
    warning.setInformativeText(tr("What you delete is removed from the sync folder on the server and goes to the "
                                  "trash on this computer, and every other computer that syncs with it moves its "
                                  "copy to the trash at its next sync."));
    if (paths.size() > 1) {
        QStringList names;
        for (const QString &path : std::as_const(paths))
            names << QDir::toNativeSeparators(path);
        warning.setDetailedText(names.join(QLatin1Char('\n')));
    }
    QPushButton *confirm = warning.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    QPushButton *cancel = warning.addButton(QMessageBox::Cancel);
    warning.setDefaultButton(cancel);
    warning.setEscapeButton(cancel);
    warning.exec();
    if (warning.clickedButton() != confirm)
        return;

    QString error;
    const int recorded = m_sync->deleteEverywhere(paths, &error);
    if (!error.isEmpty())
        QMessageBox::warning(this, tr("Could Not Delete the Files"), error);
    if (recorded == 0) {
        updateButtons();
        return;
    }
    // The sync takes them off the server and tells the other devices.
    m_waiting = true;
    setBusy(true, tr("Deleting…"));
    m_sync->sync();
}
