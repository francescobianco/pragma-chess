#include "ManageEnginesDialog.h"

#include "app/EngineDetector.h"
#include "app/UciEngine.h"

#include <QComboBox>
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

ManageEnginesDialog::ManageEnginesDialog(const EngineCatalog &catalog, const QString &activeId,
                                         const QString &pragmaDir, QWidget *parent)
    : QDialog(parent)
    , m_catalog(catalog)
    , m_pragmaDir(pragmaDir)
    , m_activeId(activeId)
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
    // How much of the computer the engine is given: never a weaker search,
    // a slower one (fewer threads, a cap on its CPU, a lower priority).
    m_power = new QComboBox(this);
    m_power->addItem(tr("Minimum: a tenth of the computer"), int(EnginePower::Minimum));
    m_power->addItem(tr("Light: a quarter of the computer"), int(EnginePower::Light));
    m_power->addItem(tr("Medium: half of the computer"), int(EnginePower::Medium));
    m_power->addItem(tr("High: three quarters of the computer"), int(EnginePower::High));
    m_power->addItem(tr("Full: the whole computer, no limit"), int(EnginePower::Full));
    m_power->setToolTip(UciEngine::canLimitCpu()
                            ? tr("The share of the processor the engine may use while it analyzes. It is just "
                                 "as strong, only slower: the rest of the computer stays free.")
                            : tr("The share of the processor the engine may use while it analyzes. It is just "
                                 "as strong, only slower. This system has no hard cap for another program: "
                                 "the engine gets fewer threads and a lower priority."));
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
    // The field is a row (path and Browse…): the label needs its buddy set by
    // hand, or it shows the "&" instead of underlining the letter.
    auto *pathLabel = new QLabel(tr("&Executable:"), this);
    pathLabel->setBuddy(m_path);
    form->addRow(pathLabel, pathRow);
    form->addRow(tr("Computing &Power:"), m_power);
    form->addRow(tr("&Threads:"), m_threads);
    form->addRow(tr("&Hash:"), m_hash);
    form->addRow(QString(), m_status);
    // Under the engine: the way to switch to it, or the word that it is the one.
    m_use = new QPushButton(tr("&Use This Engine"), this);
    m_use->setAutoDefault(false);
    m_use->setToolTip(tr("Analyze and train with this engine in the open project"));
    m_inUse = new QLabel(tr("This is the engine in use."), this);
    m_inUse->setForegroundRole(QPalette::PlaceholderText);
    auto *useRow = new QHBoxLayout;
    useRow->addWidget(m_use);
    useRow->addWidget(m_inUse);
    useRow->addStretch();
    form->addRow(QString(), useRow);

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
    connect(m_power, &QComboBox::currentIndexChanged, this, &ManageEnginesDialog::storeEngine);
    for (QSpinBox *spin : {m_threads, m_hash})
        connect(spin, &QSpinBox::valueChanged, this, &ManageEnginesDialog::storeEngine);
    connect(add, &QPushButton::clicked, this, &ManageEnginesDialog::addEngine);
    connect(m_remove, &QPushButton::clicked, this, &ManageEnginesDialog::removeEngine);
    connect(m_browse, &QPushButton::clicked, this, &ManageEnginesDialog::browse);
    connect(m_detect, &QPushButton::clicked, this, &ManageEnginesDialog::detect);
    connect(m_use, &QPushButton::clicked, this, &ManageEnginesDialog::useEngine);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildList(activeId);
}

void ManageEnginesDialog::rebuildList(const QString &selectId)
{
    m_updating = true;
    m_list->clear();
    int row = 0;
    for (int i = 0; i < m_catalog.engines().size(); ++i) {
        const EngineProfile &profile = m_catalog.engines().at(i);
        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, profile.id);
        if (profile.id == selectId)
            row = i;
    }
    m_updating = false;
    m_list->setCurrentRow(row);
    showEngine();
}

void ManageEnginesDialog::showActive()
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const EngineProfile *profile = m_catalog.find(item->data(Qt::UserRole).toString());
        if (!profile)
            continue;
        const QString name = profile->bundled ? tr("%1 (included)").arg(profile->name) : profile->name;
        const bool active = profile->id == m_activeId;
        item->setText(active ? tr("%1 — in use").arg(name) : name);
        QFont font = m_list->font();
        font.setBold(active);
        item->setFont(font);
    }
    const EngineProfile *shown = m_catalog.find(m_shownId);
    const bool active = shown && shown->id == m_activeId;
    m_inUse->setVisible(active);
    m_use->setVisible(shown && !active);
    // An engine that cannot be started is not one to switch to.
    m_use->setEnabled(shown && !EngineCatalog::executableFor(*shown).isEmpty());
}

void ManageEnginesDialog::useEngine()
{
    if (!m_catalog.find(m_shownId))
        return;
    m_activeId = m_shownId;
    showActive();
    Q_EMIT useEngineRequested();
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
    m_power->setEnabled(profile);
    m_threads->setEnabled(profile);
    m_hash->setEnabled(profile);
    m_power->setCurrentIndex(qMax(0, m_power->findData(profile ? profile->power : int(EnginePower::kDefault))));
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
    showActive();
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
    profile.power = m_power->currentData().toInt();
    profile.threads = m_threads->value();
    profile.hashMb = m_hash->value();
    m_catalog.update(profile);
    showActive(); // Its name in the list, and whether it can be used now.
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
    if (!m_catalog.remove(m_shownId))
        return;
    // Without the engine in use, the application falls back to the bundled one.
    if (!m_catalog.find(m_activeId))
        m_activeId = EngineCatalog::kBundledId;
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
