#include "ConvertPgnDialog.h"

#include "app/UserFolders.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

namespace {

QWidget *withBrowse(QLineEdit *field, QPushButton *browse, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(field, 1);
    layout->addWidget(browse);
    return row;
}

QString megabytes(qint64 bytes)
{
    return QLocale().toString(double(bytes) / (1024 * 1024), 'f', bytes < 10 * 1024 * 1024 ? 1 : 0);
}

} // namespace

ConvertPgnDialog::ConvertPgnDialog(QWidget *parent)
    : QDialog(parent)
    , m_pgn(new QLineEdit(this))
    , m_pdb(new QLineEdit(this))
    , m_browsePgn(new QPushButton(tr("Browse…"), this))
    , m_browsePdb(new QPushButton(tr("Browse…"), this))
    , m_bar(new QProgressBar(this))
    , m_info(new QLabel(this))
{
    setWindowTitle(tr("PGN to Pragma Database"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Makes a Pragma database of the games of a PGN file, however large. "
                                "The PGN file is not changed."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout;
    form->addRow(tr("PGN file:"), withBrowse(m_pgn, m_browsePgn, this));
    form->addRow(tr("Database:"), withBrowse(m_pdb, m_browsePdb, this));
    layout->addLayout(form);
    m_pgn->setMinimumWidth(360);

    m_bar->setRange(0, 1000);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    layout->addWidget(m_bar);
    m_info->setWordWrap(true);
    layout->addWidget(m_info);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    m_open = buttons->addButton(tr("Open Database"), QDialogButtonBox::ActionRole);
    m_convert = buttons->addButton(tr("Convert"), QDialogButtonBox::AcceptRole);
    m_close = buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);
    m_convert->setDefault(true);

    connect(m_browsePgn, &QPushButton::clicked, this, &ConvertPgnDialog::choosePgn);
    connect(m_browsePdb, &QPushButton::clicked, this, &ConvertPgnDialog::choosePdb);
    connect(m_pgn, &QLineEdit::textChanged, this, [this](const QString &path) {
        if (!m_pdbChosen && !path.isEmpty())
            m_pdb->setText(QDir(UserFolders::databasesDir()).filePath(QFileInfo(path).completeBaseName()
                                                                      + QStringLiteral(".pdb")));
        updateButtons();
    });
    connect(m_pdb, &QLineEdit::textEdited, this, [this] { m_pdbChosen = true; });
    connect(m_pdb, &QLineEdit::textChanged, this, &ConvertPgnDialog::updateButtons);
    connect(m_convert, &QPushButton::clicked, this, &ConvertPgnDialog::start);
    connect(m_open, &QPushButton::clicked, this, [this] { Q_EMIT openRequested(m_status.pdbPath); });
    connect(m_close, &QPushButton::clicked, this, &ConvertPgnDialog::reject);
    updateButtons();
}

ConvertPgnDialog::~ConvertPgnDialog()
{
    if (m_thread) {
        m_cancel->store(true);
        m_thread->wait();
    }
}

void ConvertPgnDialog::setPaths(const QString &pgnPath, const QString &pdbPath)
{
    m_pgn->setText(pgnPath);
    if (!pdbPath.isEmpty()) {
        m_pdb->setText(pdbPath);
        m_pdbChosen = true;
    }
}

void ConvertPgnDialog::choosePgn()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("PGN File"), QFileInfo(m_pgn->text()).absolutePath(),
                                                      tr("PGN files (*.pgn);;All files (*)"));
    if (!path.isEmpty())
        m_pgn->setText(path);
}

void ConvertPgnDialog::choosePdb()
{
    const QString start = m_pdb->text().isEmpty() ? UserFolders::databasesDir() : m_pdb->text();
    QString path = QFileDialog::getSaveFileName(this, tr("New Database"), start,
                                                tr("Pragma Chess databases (*.pdb)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".pdb"), Qt::CaseInsensitive))
        path += QStringLiteral(".pdb");
    m_pdb->setText(path);
    m_pdbChosen = true;
}

bool ConvertPgnDialog::start()
{
    if (m_thread)
        return false;
    const QString pgn = m_pgn->text().trimmed();
    const QString pdb = m_pdb->text().trimmed();
    if (!QFileInfo(pgn).isFile()) {
        m_info->setText(tr("The PGN file was not found."));
        return false;
    }
    if (QFileInfo::exists(pdb)) {
        m_info->setText(tr("“%1” already exists: choose another name.").arg(QFileInfo(pdb).fileName()));
        return false;
    }
    m_status = Status{};
    m_status.running = true;
    m_status.pgnPath = pgn;
    m_status.pdbPath = QFileInfo(pdb).absoluteFilePath();
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_bar->setValue(0);
    m_info->setText(tr("Converting…"));
    m_thread = QThread::create([this, pgn, pdb = m_status.pdbPath, cancel = m_cancel] {
        const PgnConversion::Result result = PgnConversion::run(
            pgn, pdb,
            [this](const PgnConversion::Progress &progress) {
                QMetaObject::invokeMethod(this, [this, progress] { showProgress(progress); }, Qt::QueuedConnection);
            },
            cancel.get());
        QMetaObject::invokeMethod(this, [this, result] { finished(result); }, Qt::QueuedConnection);
    });
    m_thread->start(QThread::LowPriority);
    updateButtons();
    return true;
}

void ConvertPgnDialog::showProgress(const PgnConversion::Progress &progress)
{
    m_status.progress = progress;
    if (progress.totalBytes > 0)
        m_bar->setValue(int(1000 * progress.bytesRead / progress.totalBytes));
    const qint64 perSecond = progress.elapsedMs > 0 ? progress.games * 1000 / progress.elapsedMs : 0;
    m_info->setText(tr("%1 games · %2 of %3 MB read · %4 games a second")
                        .arg(QLocale().toString(progress.games), megabytes(progress.bytesRead),
                             megabytes(progress.totalBytes), QLocale().toString(perSecond)));
}

void ConvertPgnDialog::finished(const PgnConversion::Result &result)
{
    if (m_thread) {
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_status.running = false;
    m_status.finished = true;
    m_status.result = result;
    m_status.progress = result.progress;
    const PgnConversion::Progress &done = result.progress;
    if (result.ok) {
        m_bar->setValue(m_bar->maximum());
        QString text = tr("Done: %1 games in %2 seconds.")
                           .arg(QLocale().toString(done.games), QLocale().toString(done.elapsedMs / 1000.0, 'f', 1));
        if (done.skipped > 0)
            text += QLatin1Char(' ') + tr("%n entry(ies) of the file could not be read as a game.", nullptr, int(done.skipped));
        m_info->setText(text);
    } else if (result.cancelled) {
        m_bar->setValue(0);
        m_info->setText(tr("Cancelled: no database was made."));
    } else {
        m_info->setText(tr("The conversion failed: %1").arg(result.error));
    }
    updateButtons();
}

void ConvertPgnDialog::updateButtons()
{
    const bool running = m_thread != nullptr;
    m_convert->setEnabled(!running && !m_pgn->text().trimmed().isEmpty() && !m_pdb->text().trimmed().isEmpty());
    m_open->setVisible(m_status.finished && m_status.result.ok);
    m_close->setText(running ? tr("Cancel") : tr("Close"));
    m_pgn->setEnabled(!running);
    m_pdb->setEnabled(!running);
    m_browsePgn->setEnabled(!running);
    m_browsePdb->setEnabled(!running);
}

void ConvertPgnDialog::reject()
{
    if (m_thread) {
        m_cancel->store(true); // finished() comes back with the cancel.
        m_info->setText(tr("Cancelling…"));
        return;
    }
    QDialog::reject();
}
