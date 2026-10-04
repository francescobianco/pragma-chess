#include "SourceSettingsWidget.h"

#include "app/chessbase/ChessBaseDatabase.h"
#include "app/sources/ChessBaseFetch.h"
#include "app/sources/LichessSignIn.h"
#include "app/sources/PgnFileFetch.h"
#include "app/sources/SourceCredentials.h"
#include "app/sources/TorneiOnlineFetch.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
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

    if (m_kind.needsSignIn || m_kind.canSignIn) {
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

    m_account->setClearButtonEnabled(true);
    if (isPgn()) {
        addPgnFile(form);
        return;
    }
    if (m_kind.localFile) {
        // A file on this computer: chosen with the file dialog, the name shown as the account.
        m_path = new QLineEdit;
        m_path->setPlaceholderText(tr("The .cbh file of the database"));
        auto *browse = new QPushButton(tr("&Browse…"));
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString chosen = QFileDialog::getOpenFileName(this, tr("Choose a ChessBase Database"),
                                                                m_path->text().isEmpty() ? QDir::homePath() : m_path->text(),
                                                                tr("ChessBase databases (*.cbh *.CBH)"));
            if (!chosen.isEmpty())
                m_path->setText(chosen);
        });
        auto *row = new QHBoxLayout;
        row->addWidget(m_path, 1);
        row->addWidget(browse);
        auto *fileLabel = new QLabel(tr("&File:"));
        fileLabel->setBuddy(m_path); // The row is a layout: the label needs it for its shortcut.
        form->addRow(fileLabel, row);
        connect(m_path, &QLineEdit::textChanged, this, &SourceSettingsWidget::changed);
        m_account->hide();
        m_limitSince->hide();
        m_since->hide();
        m_ratedOnly->hide();
        auto *note = new QLabel(tr("The games are copied into this database; the ChessBase files stay where they "
                                   "are and are read again while the database is open, so games added to them "
                                   "later arrive too. On another computer the file will not be there: the sync "
                                   "says so and can leave the source alone there."));
        note->setWordWrap(true);
        note->setEnabled(false);
        form->addRow(QString(), note);
        return;
    }
    if (m_kind.playerId) {
        m_idType = new QComboBox;
        m_idType->addItem(tr("ID FIDE"), QStringLiteral("fide"));
        m_idType->addItem(tr("ID FSI"), QStringLiteral("fsi"));
        form->addRow(tr("&Search by:"), m_idType);
        m_account->setPlaceholderText(tr("Player ID number"));
        // Keep only the digits, so pasting " 896489 " or "ID 896489" works.
        connect(m_account, &QLineEdit::textChanged, this, [this](const QString &text) {
            static const QRegularExpression notDigit(QStringLiteral("\\D"));
            QString digits = text;
            digits.remove(notDigit);
            if (digits != text) {
                const QSignalBlocker blocker(m_account);
                m_account->setText(digits);
            }
        });
        form->addRow(tr("&Player ID:"), m_account);
        connect(m_idType, &QComboBox::currentIndexChanged, this, &SourceSettingsWidget::changed);
    } else {
        m_account->setPlaceholderText(tr("Username on %1").arg(m_kind.name));
        form->addRow(tr("&Account:"), m_account);
    }

    m_since->setCalendarPopup(true);
    m_since->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    m_since->setEnabled(false);
    auto *sinceRow = new QHBoxLayout;
    sinceRow->addWidget(m_limitSince);
    sinceRow->addWidget(m_since);
    sinceRow->addStretch();
    if (m_kind.playerId) {
        m_limitSince->setText(tr("Only tournaments started since"));
        form->addRow(tr("Tournaments:"), sinceRow);
        m_ratedOnly->hide();
    } else {
        form->addRow(tr("Games:"), sinceRow);
        form->addRow(QString(), m_ratedOnly);
    }

    auto *note = new QLabel(m_kind.playerId ? tr("The site has no moves: each game is added with its players, "
                                                 "round and result, ready for the moves to be entered. New "
                                                 "tournaments are added while the database is open.")
                                            : tr("New games are added while the database is open."));
    note->setWordWrap(true);
    note->setEnabled(false);
    form->addRow(QString(), note);

    connect(m_limitSince, &QCheckBox::toggled, m_since, &QWidget::setEnabled);
    connect(m_account, &QLineEdit::textChanged, this, &SourceSettingsWidget::changed);
}

void SourceSettingsWidget::addPgnFile(QFormLayout *form)
{
    m_path = new QLineEdit;
    m_path->setPlaceholderText(tr("A .pgn file on this computer"));
    m_path->setClearButtonEnabled(true);
    const auto startFolder = [this] {
        return m_path->text().isEmpty() ? QDir::homePath() : QFileInfo(m_path->text()).absolutePath();
    };
    auto *browse = new QPushButton(tr("&Browse…"));
    connect(browse, &QPushButton::clicked, this, [this, startFolder] {
        const QString chosen = QFileDialog::getOpenFileName(this, tr("Choose a PGN File"), startFolder(),
                                                            tr("PGN files (*.pgn *.PGN)"));
        if (!chosen.isEmpty())
            m_path->setText(QDir::toNativeSeparators(chosen));
    });
    auto *create = new QPushButton(tr("&New File…"));
    create->setToolTip(tr("Creates an empty PGN file, for the database to write its games into"));
    connect(create, &QPushButton::clicked, this, [this, startFolder] {
        QString chosen = QFileDialog::getSaveFileName(this, tr("New PGN File"), startFolder(),
                                                      tr("PGN files (*.pgn *.PGN)"));
        if (chosen.isEmpty())
            return;
        if (QFileInfo(chosen).suffix().isEmpty())
            chosen += QStringLiteral(".pgn");
        QFile file(chosen);
        // An existing file is kept as it is: the file dialog already asked about it.
        if (!file.exists() && !file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("New PGN File"),
                                 tr("Could not create “%1”: %2").arg(QDir::toNativeSeparators(chosen), file.errorString()));
            return;
        }
        m_path->setText(QDir::toNativeSeparators(chosen));
    });
    auto *row = new QHBoxLayout;
    row->addWidget(m_path, 1);
    row->addWidget(browse);
    row->addWidget(create);
    auto *fileLabel = new QLabel(tr("&File:"));
    fileLabel->setBuddy(m_path); // The row is a layout: the label needs it for its shortcut.
    form->addRow(fileLabel, row);
    connect(m_path, &QLineEdit::textChanged, this, &SourceSettingsWidget::changed);

    m_direction = new QComboBox;
    m_direction->addItem(tr("Read and write"), PgnFilePlan::modeKey(PgnFilePlan::Mode::ReadWrite));
    m_direction->addItem(tr("Read only"), PgnFilePlan::modeKey(PgnFilePlan::Mode::Read));
    m_direction->addItem(tr("Write only"), PgnFilePlan::modeKey(PgnFilePlan::Mode::Write));
    form->addRow(tr("&Direction:"), m_direction);
    auto *explanation = new QLabel;
    explanation->setWordWrap(true);
    form->addRow(QString(), explanation);
    const auto explain = [this, explanation] {
        switch (PgnFilePlan::modeFromKey(m_direction->currentData().toString())) {
        case PgnFilePlan::Mode::ReadWrite:
            explanation->setText(tr("The games of the file come into the database, and the database's games go "
                                    "into the file: a game added or changed on either side reaches the other."));
            break;
        case PgnFilePlan::Mode::Read:
            explanation->setText(tr("The games of the file come into the database, and so do the ones added or "
                                    "changed in it later. The file is never written."));
            break;
        case PgnFilePlan::Mode::Write:
            explanation->setText(tr("Every game of the database goes into the file, and so do the ones added or "
                                    "changed later. The file's other games are left as they are."));
            break;
        }
    };
    connect(m_direction, &QComboBox::currentIndexChanged, this, explain);
    connect(m_direction, &QComboBox::currentIndexChanged, this, &SourceSettingsWidget::changed);
    explain();

    m_account->hide();
    m_limitSince->hide();
    m_since->hide();
    m_ratedOnly->hide();
    auto *note = new QLabel(tr("The file stays where it is. A game removed on one side is not removed on the "
                               "other, and a game changed on both sides between two syncs is kept twice. "
                               "Pragma Chess marks the games it writes with a PragmaUid tag and keeps an index "
                               "of the file beside it, in a hidden file."));
    note->setWordWrap(true);
    note->setEnabled(false);
    form->addRow(QString(), note);
}

void SourceSettingsWidget::setSource(const GameSource &source)
{
    m_account->setText(source.account);
    if (m_path)
        m_path->setText(QDir::toNativeSeparators(SourceCatalog::localPath(source)));
    if (m_direction)
        m_direction->setCurrentIndex(qMax(0, m_direction->findData(PgnFilePlan::modeKey(PgnFileFetch::mode(source)))));
    const QString since = source.settings.value(QLatin1String(SourceSettings::since)).toString();
    m_limitSince->setChecked(!since.isEmpty());
    if (!since.isEmpty())
        m_since->setDate(QDate::fromString(since, Qt::ISODate));
    m_ratedOnly->setChecked(source.settings.value(QLatin1String(SourceSettings::ratedOnly)).toBool());
    if (m_idType) {
        const int index = m_idType->findData(source.settings.value(QLatin1String(TorneiOnlineSettings::idType)).toString());
        m_idType->setCurrentIndex(qMax(index, 0));
        m_player = source.settings.value(QLatin1String(TorneiOnlineSettings::player)).toString();
    }
}

void SourceSettingsWidget::applyTo(GameSource &source) const
{
    if (m_path) {
        const QString path = QFileInfo(QDir::fromNativeSeparators(m_path->text().trimmed())).absoluteFilePath();
        source.account = QFileInfo(path).completeBaseName();
        source.settings.insert(QLatin1String(ChessBaseSettings::path), path);
        if (m_direction)
            source.settings.insert(QLatin1String(PgnFileSettings::mode), m_direction->currentData().toString());
        return;
    }
    source.account = m_account->text().trimmed();
    source.settings.remove(QLatin1String(SourceSettings::since));
    if (m_limitSince->isChecked())
        source.settings.insert(QLatin1String(SourceSettings::since), m_since->date().toString(Qt::ISODate));
    if (m_idType) {
        source.settings.insert(QLatin1String(TorneiOnlineSettings::idType), m_idType->currentData().toString());
        source.settings.insert(QLatin1String(TorneiOnlineSettings::player), m_player);
    } else {
        source.settings.insert(QLatin1String(SourceSettings::ratedOnly), m_ratedOnly->isChecked());
    }
}

bool SourceSettingsWidget::validate(QString *errorMessage)
{
    if (m_path && isPgn()) {
        const QFileInfo file(QDir::fromNativeSeparators(m_path->text().trimmed()));
        if (m_path->text().trimmed().isEmpty()) {
            *errorMessage = tr("Choose the PGN file, or create one with New File….");
            return false;
        }
        if (!file.isFile()) {
            *errorMessage = tr("There is no file “%1”. New File… creates one.").arg(m_path->text().trimmed());
            return false;
        }
        if (!file.isReadable()) {
            *errorMessage = tr("The file “%1” cannot be read.").arg(m_path->text().trimmed());
            return false;
        }
        if (PgnFilePlan::writes(PgnFilePlan::modeFromKey(m_direction->currentData().toString())) && !file.isWritable()) {
            *errorMessage = tr("The file “%1” cannot be written: choose Read only, or another file.")
                                .arg(m_path->text().trimmed());
            return false;
        }
        return true;
    }
    if (m_path) {
        const QString path = m_path->text().trimmed();
        if (path.isEmpty()) {
            *errorMessage = tr("Choose the .cbh file of the ChessBase database.");
            return false;
        }
        QString why;
        const std::unique_ptr<ChessBaseDatabase> database = ChessBaseDatabase::open(path, &why);
        if (!database) {
            *errorMessage = why;
            return false;
        }
        return true;
    }
    const QString account = m_account->text().trimmed();
    if (account.isEmpty()) {
        *errorMessage = m_kind.playerId ? tr("Enter the ID of the player whose games to import.")
                                        : tr("Enter the account whose games to import.");
        return false;
    }
    if (m_kind.needsSignIn && SourceCredentials::token(m_uuid).isEmpty()) {
        *errorMessage = tr("Sign in to %1 first.").arg(m_kind.name);
        return false;
    }

    QNetworkAccessManager network;
    GameSource source;
    source.kind = m_kind.id;
    applyTo(source);
    QNetworkRequest request = SourceCatalog::accountRequest(source);
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
    if (m_idType) {
        const std::optional<TorneiOnlineFetch::Player> player =
            TorneiOnlineFetch::parsePlayerSearch(TorneiOnlineFetch::decodePage(reply->readAll()), account);
        if (!player) {
            *errorMessage = tr("There is no player with %1 “%2” on %3.").arg(m_idType->currentText(), account, m_kind.name);
            return false;
        }
        m_player = player->name;
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
    QTimer::singleShot(0, &flow, [&flow] { flow.start(); });
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
    m_signInStatus->setText(signedIn             ? tr("Signed in to %1").arg(m_kind.name)
                            : m_kind.needsSignIn ? tr("Not signed in")
                                                 : tr("Not signed in: public games only"));
    m_signInButton->setText(signedIn ? tr("Sign In Again…") : tr("Sign In with %1…").arg(m_kind.name));
}
