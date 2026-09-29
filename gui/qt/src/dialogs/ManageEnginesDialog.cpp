#include "ManageEnginesDialog.h"

#include "app/EngineDetector.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

ManageEnginesDialog::ManageEnginesDialog(const EngineCatalog &catalog, const QString &selectedId,
                                         const QString &pragmaDir, QWidget *parent)
    : QDialog(parent)
    , m_catalog(catalog)
    , m_pragmaDir(pragmaDir)
{
    setWindowTitle(tr("Manage Engines"));
    resize(640, 380);

    m_list = new QListWidget(this);
    m_list->setMinimumWidth(200);

    auto *add = new QPushButton(tr("&Add"), this);
    m_remove = new QPushButton(tr("&Remove"), this);
    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(add);
    listButtons->addWidget(m_remove);
    listButtons->addStretch();
    auto *listColumn = new QVBoxLayout;
    listColumn->addWidget(m_list);
    listColumn->addLayout(listButtons);

    m_name = new QLineEdit(this);
    m_path = new QLineEdit(this);
    m_browse = new QPushButton(tr("&Browse…"), this);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path);
    pathRow->addWidget(m_browse);
    m_threads = new QSpinBox(this);
    m_threads->setRange(0, 1024);
    m_threads->setSpecialValueText(tr("Automatic"));
    m_hash = new QSpinBox(this);
    m_hash->setRange(0, 1 << 20);
    m_hash->setSuffix(tr(" MB"));
    m_hash->setSpecialValueText(tr("Engine default"));
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *form = new QFormLayout;
    form->addRow(tr("&Name:"), m_name);
    form->addRow(tr("&Executable:"), pathRow);
    form->addRow(tr("&Threads:"), m_threads);
    form->addRow(tr("&Hash:"), m_hash);
    form->addRow(QString(), m_status);

    auto *columns = new QHBoxLayout;
    columns->addLayout(listColumn, 2);
    columns->addLayout(form, 3);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_detect = buttons->addButton(tr("&Detect Engines"), QDialogButtonBox::ActionRole);
    m_detect->setToolTip(tr("Look for UCI engines installed on this computer"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(columns);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, &ManageEnginesDialog::showEngine);
    for (QLineEdit *edit : {m_name, m_path})
        connect(edit, &QLineEdit::textEdited, this, &ManageEnginesDialog::storeEngine);
    for (QSpinBox *spin : {m_threads, m_hash})
        connect(spin, &QSpinBox::valueChanged, this, &ManageEnginesDialog::storeEngine);
    connect(add, &QPushButton::clicked, this, &ManageEnginesDialog::addEngine);
    connect(m_remove, &QPushButton::clicked, this, &ManageEnginesDialog::removeEngine);
    connect(m_browse, &QPushButton::clicked, this, &ManageEnginesDialog::browse);
    connect(m_detect, &QPushButton::clicked, this, &ManageEnginesDialog::detect);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildList(selectedId);
}

QString ManageEnginesDialog::selectedId() const
{
    return m_shownId;
}

void ManageEnginesDialog::rebuildList(const QString &selectId)
{
    m_updating = true;
    m_list->clear();
    int row = 0;
    for (int i = 0; i < m_catalog.engines().size(); ++i) {
        const EngineProfile &profile = m_catalog.engines().at(i);
        auto *item = new QListWidgetItem(profile.bundled ? tr("%1 (included)").arg(profile.name) : profile.name,
                                         m_list);
        item->setData(Qt::UserRole, profile.id);
        if (profile.id == selectId)
            row = i;
    }
    m_updating = false;
    m_list->setCurrentRow(row);
    showEngine();
}

void ManageEnginesDialog::showEngine()
{
    if (m_updating)
        return;
    const QListWidgetItem *item = m_list->currentItem();
    const EngineProfile *profile = item ? m_catalog.find(item->data(Qt::UserRole).toString()) : nullptr;
    m_shownId = profile ? profile->id : QString();

    m_updating = true;
    const bool editable = profile && !profile->bundled;
    m_name->setText(profile ? profile->name : QString());
    m_name->setReadOnly(!editable);
    m_path->setReadOnly(!editable);
    m_browse->setEnabled(editable);
    m_remove->setEnabled(editable);
    m_threads->setEnabled(profile);
    m_hash->setEnabled(profile);
    m_threads->setValue(profile ? profile->threads : 0);
    m_hash->setValue(profile ? profile->hashMb : 0);
    if (profile && profile->bundled) {
        const QString path = EngineCatalog::bundledEnginePath();
        const QString executable = EngineCatalog::executableFor(*profile);
        m_path->setText(executable);
        m_status->setText(!path.isEmpty() ? tr("Included with Pragma Chess.")
                          : !executable.isEmpty()
                              ? tr("This build does not include an engine: using the Stockfish installed on "
                                   "this computer.")
                              : tr("This build does not include an engine and none was found on this computer."));
    } else {
        m_path->setText(profile ? profile->path : QString());
        const bool found = profile && !EngineCatalog::executableFor(*profile).isEmpty();
        m_status->setText(!profile || found ? QString() : tr("The executable was not found."));
    }
    m_updating = false;
}

void ManageEnginesDialog::storeEngine()
{
    if (m_updating || m_shownId.isEmpty())
        return;
    const EngineProfile *existing = m_catalog.find(m_shownId);
    if (!existing)
        return;
    EngineProfile profile = *existing;
    if (!profile.bundled) {
        profile.name = m_name->text().trimmed();
        profile.path = m_path->text().trimmed();
    }
    profile.threads = m_threads->value();
    profile.hashMb = m_hash->value();
    m_catalog.update(profile);
    if (QListWidgetItem *item = m_list->currentItem(); item && !profile.bundled)
        item->setText(profile.name);
}

void ManageEnginesDialog::addEngine()
{
    EngineProfile profile;
    profile.name = tr("New Engine");
    rebuildList(m_catalog.add(profile));
    m_name->setFocus();
    m_name->selectAll();
}

void ManageEnginesDialog::removeEngine()
{
    if (m_catalog.remove(m_shownId))
        rebuildList(EngineCatalog::kBundledId);
}

void ManageEnginesDialog::browse()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose Engine"),
                                                      m_path->text().isEmpty() ? m_pragmaDir : m_path->text());
    if (path.isEmpty())
        return;
    m_path->setText(path);
    if (m_name->text().isEmpty() || m_name->text() == tr("New Engine"))
        m_name->setText(QFileInfo(path).completeBaseName());
    storeEngine();
    showEngine();
}

void ManageEnginesDialog::detect()
{
    m_detect->setEnabled(false);
    m_status->setText(tr("Looking for engines…"));
    // Each candidate is started once for the UCI handshake: keep it off the GUI thread.
    auto *found = new QList<DetectedEngine>;
    QThread *thread = QThread::create([found, dir = m_pragmaDir] { *found = EngineDetector::scan(dir); });
    QPointer<ManageEnginesDialog> self(this);
    connect(thread, &QThread::finished, thread, [self, thread, found] {
        thread->deleteLater();
        const QList<DetectedEngine> engines = *found;
        delete found;
        if (!self)
            return;
        const int added = self->m_catalog.addDetected(engines, EngineCatalog::bundledEnginePath());
        const QString selected = self->m_shownId;
        self->rebuildList(added > 0 ? self->m_catalog.engines().last().id : selected);
        self->m_status->setText(added == 0 ? tr("No new engines found.")
                                           : tr("Found %n new engine(s).", nullptr, added));
        self->m_detect->setEnabled(true);
    });
    thread->start();
}
