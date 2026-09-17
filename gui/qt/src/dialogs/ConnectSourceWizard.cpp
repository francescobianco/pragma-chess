#include "ConnectSourceWizard.h"

#include "SourceSettingsWidget.h"
#include "app/sources/SourceCatalog.h"
#include "app/sources/SourceCredentials.h"

#include <QLabel>
#include <QListWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QWizardPage>

ConnectSourceWizard::ConnectSourceWizard(const QString &databaseName, QWidget *parent)
    : QWizard(parent)
    , m_databaseName(databaseName)
    , m_uuid(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_kinds(new QListWidget)
    , m_settingsLayout(new QVBoxLayout)
    , m_settingsError(new QLabel)
    , m_summary(new QLabel)
{
    setWindowTitle(tr("Connect Source"));
    setModal(true);
    setOption(QWizard::NoBackButtonOnStartPage);
    setMinimumWidth(560);

    auto *choose = new QWizardPage;
    choose->setTitle(tr("Connect a Source to “%1”").arg(databaseName));
    choose->setSubTitle(tr("Games from the source are added to this database and kept up to date "
                           "in the background while it is open."));
    auto *chooseLayout = new QVBoxLayout(choose);
    chooseLayout->addWidget(new QLabel(tr("&Source:")));
    m_kinds->setAccessibleName(tr("Source"));
    m_kinds->setWordWrap(true);
    m_kinds->setSpacing(2);
    for (const SourceKind &kind : SourceCatalog::kinds()) {
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(kind.name, kind.description), m_kinds);
        item->setData(Qt::UserRole, kind.id);
        item->setToolTip(kind.description);
    }
    m_kinds->setCurrentRow(0);
    qobject_cast<QLabel *>(chooseLayout->itemAt(0)->widget())->setBuddy(m_kinds);
    chooseLayout->addWidget(m_kinds);
    connect(m_kinds, &QListWidget::itemDoubleClicked, this, &QWizard::next);
    setPage(ChoosePage, choose);

    auto *settings = new QWizardPage;
    auto *settingsLayout = new QVBoxLayout(settings);
    settingsLayout->addLayout(m_settingsLayout);
    m_settingsError->setWordWrap(true);
    m_settingsError->hide();
    settingsLayout->addWidget(m_settingsError);
    settingsLayout->addStretch();
    setPage(SettingsPage, settings);

    auto *summary = new QWizardPage;
    summary->setTitle(tr("Ready to Connect"));
    summary->setFinalPage(true);
    auto *summaryLayout = new QVBoxLayout(summary);
    m_summary->setWordWrap(true);
    summaryLayout->addWidget(m_summary);
    summaryLayout->addStretch();
    setPage(SummaryPage, summary);
}

ConnectSourceWizard::~ConnectSourceWizard() = default;

QString ConnectSourceWizard::chosenKind() const
{
    const QListWidgetItem *item = m_kinds->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ConnectSourceWizard::initializePage(int id)
{
    if (id == SettingsPage && chosenKind() != m_settingsKind) {
        // Each kind has its own settings; rebuild them when the choice changes.
        delete m_settings;
        const SourceKind kind = *SourceCatalog::kind(chosenKind());
        m_settings = new SourceSettingsWidget(kind, m_uuid);
        m_settingsLayout->addWidget(m_settings);
        m_settingsKind = kind.id;
        page(SettingsPage)->setTitle(kind.playerId ? tr("%1 Player").arg(kind.name) : tr("%1 Account").arg(kind.name));
        page(SettingsPage)->setSubTitle(tr("Which games to import into “%1”.").arg(m_databaseName));
        connect(m_settings, &SourceSettingsWidget::changed, m_settingsError, &QWidget::hide);
    }
    if (id == SummaryPage) {
        const GameSource configured = source();
        const QString since = configured.settings.value(QLatin1String(SourceSettings::since)).toString();
        const bool ratedOnly = configured.settings.value(QLatin1String(SourceSettings::ratedOnly)).toBool();
        QString games = ratedOnly ? tr("rated games") : tr("games");
        if (!since.isEmpty())
            games = tr("%1 played since %2").arg(games, QLocale().toString(QDate::fromString(since, Qt::ISODate),
                                                                            QLocale::LongFormat));
        m_summary->setText(tr("The %1 of <b>%2</b> will be imported into <b>%3</b>.<br><br>"
                              "The first import starts when you finish and runs in the background; "
                              "new games keep arriving while the database is open. Sources can be "
                              "removed or signed in again from Database ▸ Manage Sources.")
                               .arg(games.toHtmlEscaped(), SourceCatalog::displayName(configured).toHtmlEscaped(),
                                    m_databaseName.toHtmlEscaped()));
    }
    QWizard::initializePage(id);
}

bool ConnectSourceWizard::validateCurrentPage()
{
    if (currentId() == SettingsPage && m_settings) {
        QString error;
        if (!m_settings->validate(&error)) {
            m_settingsError->setText(error);
            m_settingsError->show();
            return false;
        }
        m_settingsError->hide();
    }
    return QWizard::validateCurrentPage();
}

void ConnectSourceWizard::done(int result)
{
    // A sign-in for a source that was not connected leaves no token behind.
    if (result != QDialog::Accepted)
        SourceCredentials::remove(m_uuid);
    QWizard::done(result);
}

GameSource ConnectSourceWizard::source() const
{
    GameSource source;
    source.uuid = m_uuid;
    source.kind = m_settingsKind;
    if (m_settings)
        m_settings->applyTo(source);
    return source;
}
