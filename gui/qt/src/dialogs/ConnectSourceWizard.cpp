#include "ConnectSourceWizard.h"

#include "SourceKindDelegate.h"

#include "SourceSettingsWidget.h"
#include "app/sources/SourceCatalog.h"
#include "app/sources/SourceCredentials.h"

#include <QLabel>
#include <QListWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QWizardPage>

namespace {

/// Wide enough for the descriptions of the sources on two lines or three.
constexpr int kMinimumWidth = 680;

/// A page whose contents start where the wizard's separators and buttons do,
/// under a title and a description of its own (the wizard's header indents
/// its subtitle and the page its contents, each by a different amount).
QVBoxLayout *pageLayout(QWizardPage *page, QLabel *title, QLabel *description)
{
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    QFont font = title->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.15);
    title->setFont(font);
    title->setWordWrap(true);
    description->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(description);
    layout->addSpacing(8);
    return layout;
}

} // namespace

ConnectSourceWizard::ConnectSourceWizard(const QString &databaseName, QWidget *parent)
    : QWizard(parent)
    , m_databaseName(databaseName)
    , m_uuid(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_kinds(new QListWidget)
    , m_settingsLayout(new QVBoxLayout)
    , m_settingsError(new QLabel)
    , m_summary(new QLabel)
    , m_settingsTitle(new QLabel)
    , m_settingsDescription(new QLabel)
{
    setWindowTitle(tr("Connect Source"));
    setModal(true);
    // The plain wizard everywhere, drawn with the palette like any dialog.
    // Windows' default (AeroStyle) paints its own white page whatever the
    // theme: in dark mode the texts, light, vanished on it.
    setWizardStyle(QWizard::ClassicStyle);
    setOption(QWizard::NoBackButtonOnStartPage);
    setMinimumWidth(kMinimumWidth);

    auto *choose = new QWizardPage;
    auto *chooseLayout = pageLayout(choose, new QLabel(tr("Connect a Source to “%1”").arg(databaseName)),
                                    new QLabel(tr("Games from the source are added to this database and kept up "
                                                  "to date in the background while it is open.")));
    auto *sourceLabel = new QLabel(tr("&Source:"));
    chooseLayout->addWidget(sourceLabel);
    m_kinds->setAccessibleName(tr("Source"));
    auto *delegate = new SourceKindDelegate(m_kinds);
    m_kinds->setItemDelegate(delegate);
    m_kinds->setResizeMode(QListView::Adjust); // The descriptions wrap again when the list is resized.
    m_kinds->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    for (const SourceKind &kind : SourceCatalog::kinds()) {
        auto *item = new QListWidgetItem(kind.name, m_kinds);
        item->setData(Qt::UserRole, kind.id);
        item->setData(SourceKindDelegate::kDescriptionRole, kind.description);
        item->setToolTip(kind.description);
    }
    // Tall enough to show four kinds whole at the window's smallest width.
    QStyleOptionViewItem option;
    option.initFrom(m_kinds);
    option.font = m_kinds->font();
    // The list is narrower than the window by the frame, the margins and a
    // scroll bar: measured a little narrow, so a description never wraps
    // onto a line that was not counted.
    int listHeight = 2 * m_kinds->frameWidth() + 4;
    for (int row = 0; row < qMin(4, m_kinds->count()); ++row)
        listHeight += delegate->heightFor(option, m_kinds->model()->index(row, 0), kMinimumWidth - 120);
    m_kinds->setMinimumHeight(listHeight);
    m_kinds->setCurrentRow(0);
    sourceLabel->setBuddy(m_kinds);
    chooseLayout->addWidget(m_kinds);
    connect(m_kinds, &QListWidget::itemDoubleClicked, this, &QWizard::next);
    setPage(ChoosePage, choose);

    auto *settings = new QWizardPage;
    auto *settingsLayout = pageLayout(settings, m_settingsTitle, m_settingsDescription);
    settingsLayout->addLayout(m_settingsLayout);
    m_settingsError->setWordWrap(true);
    m_settingsError->hide();
    settingsLayout->addWidget(m_settingsError);
    settingsLayout->addStretch();
    setPage(SettingsPage, settings);

    auto *summary = new QWizardPage;
    summary->setFinalPage(true);
    auto *summaryLayout = pageLayout(summary, new QLabel(tr("Ready to Connect")), new QLabel);
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
        const bool study = kind.id == QLatin1String("lichess-study");
        m_settingsTitle->setText(study            ? kind.name
                                 : kind.playerId ? tr("%1 Player").arg(kind.name)
                                                 : tr("%1 Account").arg(kind.name));
        m_settingsDescription->setText(study ? tr("Which study to keep in “%1”.").arg(m_databaseName)
                                             : tr("Which games to import into “%1”.").arg(m_databaseName));
        connect(m_settings, &SourceSettingsWidget::changed, m_settingsError, &QWidget::hide);
    }
    if (id == SummaryPage) {
        const GameSource configured = source();
        const QString since = configured.settings.value(QLatin1String(SourceSettings::since)).toString();
        const bool ratedOnly = configured.settings.value(QLatin1String(SourceSettings::ratedOnly)).toBool();
        QString games = configured.kind == QLatin1String("lichess-study") ? tr("chapters")
                        : ratedOnly                                       ? tr("rated games")
                                                                          : tr("games");
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
