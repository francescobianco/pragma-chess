#include "WelcomeDialog.h"

#include "../app/LocalizedText.h"
#include "../app/Project.h"
#include "../app/SqliteGameDatabase.h"
#include "../app/UiLanguage.h"
#include "../app/UserFolders.h"
#include "../platform/SymbolicIcons.h"
#include "../widgets/BookFont.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileSystemWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {

const QString kShowKey = QStringLiteral("welcome/showAtStartup");
constexpr int kShoulderWidth = 400;
constexpr int kLogoSize = 88;

/// The left of the window: the picture of a study with chessboards, cut to
/// fill it, darkened towards the bottom where the logo and the name stand.
class Shoulder : public QWidget
{
public:
    explicit Shoulder(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_picture(QStringLiteral(":/welcome/chess-study.png"))
        , m_logo(QStringLiteral(":/icons/hicolor/256x256/apps/io.github.francescobianco.PragmaChess.png"))
    {
        setFixedWidth(kShoulderWidth);
        setMinimumHeight(600);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF area = rect();

        // The picture covers the shoulder, centred, whatever its proportions.
        if (!m_picture.isNull()) {
            const QSizeF covered = QSizeF(m_picture.size()).scaled(area.size(), Qt::KeepAspectRatioByExpanding);
            const QRectF target(area.center() - QPointF(covered.width() / 2, covered.height() / 2), covered);
            painter.drawPixmap(target, m_picture, m_picture.rect());
        } else {
            painter.fillRect(area, QColor(0x2a, 0x21, 0x1a));
        }

        // A veil, light at the top and deep at the bottom, for the name to read on.
        QLinearGradient veil(area.topLeft(), area.bottomLeft());
        veil.setColorAt(0.0, QColor(12, 9, 6, 70));
        veil.setColorAt(0.45, QColor(12, 9, 6, 40));
        veil.setColorAt(0.72, QColor(12, 9, 6, 175));
        veil.setColorAt(1.0, QColor(12, 9, 6, 235));
        painter.fillRect(area, veil);

        const qreal margin = 32;
        QFont title = BookFont::paragraph(font());
        title.setPixelSize(58);
        QFont tagline = font();
        tagline.setPixelSize(12);
        tagline.setCapitalization(QFont::AllUppercase);
        tagline.setLetterSpacing(QFont::AbsoluteSpacing, 2.2);

        const QFontMetricsF titleMetrics(title);
        const QFontMetricsF taglineMetrics(tagline);
        const qreal lineHeight = titleMetrics.height() * 0.88;
        const qreal taglineTop = area.bottom() - margin - taglineMetrics.height();
        const qreal chessBaseline = taglineTop - 14 - titleMetrics.descent();
        const qreal pragmaBaseline = chessBaseline - lineHeight;
        const qreal logoTop = pragmaBaseline - titleMetrics.ascent() - 18 - kLogoSize;

        // The logo on a white tile, as the icon of the application shows it.
        const QRectF logoRect(margin, logoTop, kLogoSize, kLogoSize);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 90));
        painter.drawRoundedRect(logoRect.translated(0, 3).adjusted(-2, -2, 2, 2), 18, 18);
        if (!m_logo.isNull())
            painter.drawPixmap(logoRect, m_logo, m_logo.rect());

        painter.setFont(title);
        painter.setPen(QColor(0xfa, 0xf6, 0xee));
        painter.drawText(QPointF(margin - 2, pragmaBaseline), QStringLiteral("Pragma"));
        painter.drawText(QPointF(margin - 2, chessBaseline), QStringLiteral("Chess"));

        // A thin rule, as a book's title page has, then the tagline.
        painter.setPen(QPen(QColor(0xe8, 0xc9, 0x8a, 200), 1));
        const qreal ruleY = taglineTop - 6;
        painter.drawLine(QPointF(margin, ruleY), QPointF(margin + 48, ruleY));
        painter.setFont(tagline);
        painter.setPen(QColor(0xfa, 0xf6, 0xee, 200));
        painter.drawText(QRectF(margin, taglineTop, area.width() - 2 * margin, taglineMetrics.height() + 4),
                         Qt::AlignLeft | Qt::AlignTop, WelcomeDialog::tr("Study · Train · Play"));
    }

private:
    QPixmap m_picture;
    QPixmap m_logo;
};

QLabel *heading(const QString &text, qreal scale)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * scale);
    label->setFont(font);
    return label;
}

QListWidget *fileList()
{
    auto *list = new QListWidget;
    list->setIconSize(QSize(20, 20));
    list->setSpacing(2);
    list->setUniformItemSizes(true);
    // Rows with room, as a launcher's: each is a thing to open.
    QFont font = list->font();
    font.setPointSizeF(font.pointSizeF() * 1.05);
    list->setFont(font);
    list->setStyleSheet(QStringLiteral("QListWidget::item { padding: 4px 6px; }"));
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setMouseTracking(true); // The row under the pointer is highlighted: it is a menu.
    list->setCursor(Qt::PointingHandCursor);
    list->setMinimumHeight(150);
    return list;
}

} // namespace

WelcomeDialog::WelcomeDialog(QWidget *parent)
    : QDialog(parent)
    , m_projects(fileList())
    , m_databases(fileList())
    , m_dontShow(new QCheckBox(tr("Don't show this window at startup")))
    , m_watcher(new QFileSystemWatcher(this))
{
    setWindowTitle(tr("Welcome to Pragma Chess"));
    setAttribute(Qt::WA_DeleteOnClose);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(new Shoulder);

    auto *body = new QVBoxLayout;
    body->setContentsMargins(32, 28, 28, 20);
    body->setSpacing(10);
    outer->addLayout(body, 1);

    body->addWidget(heading(tr("Welcome"), 1.8));
    auto *intro = new QLabel(tr("Pragma Chess keeps your games in databases and your work in projects: "
                                "chapters of games with your moves, variations, comments and paragraphs, "
                                "as in a chess book."));
    intro->setWordWrap(true);
    body->addWidget(intro);

    // What can be done, two by two.
    struct Feature {
        QString title;
        QString text;
    };
    const QList<Feature> features{
        {tr("Explain"), tr("Press E on a move: arrows show why the evaluation changed.")},
        {tr("Train"), tr("Play the engine; the tutor stops you when a move is a mistake.")},
        {tr("Opening Tree"), tr("The book's moves for the position, and your repertoire.")},
        {tr("Databases"), tr("Import from lichess, chess.com, PGN and ChessBase; search by position.")},
        {tr("Play"), tr("Play on lichess, or in the lobby's correspondence tournaments.")},
        {tr("Sync"), tr("Keep databases and projects the same on every computer and on the phone.")},
    };
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(8);
    for (int i = 0; i < features.size(); ++i) {
        auto *label = new QLabel(QStringLiteral("<b>%1</b><br>%2").arg(features[i].title.toHtmlEscaped(),
                                                                         features[i].text.toHtmlEscaped()));
        label->setWordWrap(true);
        label->setTextFormat(Qt::RichText);
        grid->addWidget(label, i / 2, i % 2, Qt::AlignTop);
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    body->addSpacing(6);
    body->addLayout(grid);
    body->addSpacing(14);

    // The way in: projects, databases.
    auto *lists = new QGridLayout;
    lists->setHorizontalSpacing(20);
    lists->setVerticalSpacing(6);
    auto *projectsTitle = new QHBoxLayout;
    projectsTitle->addWidget(heading(tr("Projects"), 1.15));
    projectsTitle->addStretch();
    auto *newProject = new QPushButton(SymbolicIcons::icon(QStringLiteral("document-new")), tr("New Project"));
    projectsTitle->addWidget(newProject);
    lists->addLayout(projectsTitle, 0, 0);
    auto *databasesTitle = new QHBoxLayout;
    databasesTitle->addWidget(heading(tr("Databases"), 1.15));
    databasesTitle->addStretch();
    auto *databasesHint = new QLabel(tr("opens in a new project"));
    databasesHint->setEnabled(false);
    databasesTitle->addWidget(databasesHint);
    lists->addLayout(databasesTitle, 0, 1);
    // Both rows of titles as tall as the button's.
    lists->setRowMinimumHeight(0, newProject->sizeHint().height());
    lists->addWidget(m_projects, 1, 0);
    lists->addWidget(m_databases, 1, 1);
    lists->setColumnStretch(0, 1);
    lists->setColumnStretch(1, 1);
    lists->setRowStretch(1, 1);
    body->addLayout(lists, 1);

    auto *footer = new QHBoxLayout;
    m_dontShow->setChecked(!showsAtStartup());
    footer->addWidget(m_dontShow);
    footer->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    footer->addWidget(buttons);
    body->addSpacing(6);
    body->addLayout(footer);

    connect(m_dontShow, &QCheckBox::toggled, this, [](bool dontShow) {
        QSettings().setValue(kShowKey, !dontShow);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(newProject, &QPushButton::clicked, this, [this] {
        Q_EMIT newProjectChosen();
        accept();
    });
    connect(m_projects, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { choose(item, true); });
    connect(m_databases, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { choose(item, false); });

    // The lists are the folders: a file added or removed while the window is
    // open shows at once.
    for (const QString &folder : {UserFolders::projectsDir(), UserFolders::databasesDir()}) {
        if (QFileInfo::exists(folder))
            m_watcher->addPath(folder);
    }
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        fillProjects();
        fillDatabases();
    });
    fillProjects();
    fillDatabases();

    resize(1100, 680);
}

bool WelcomeDialog::showsAtStartup()
{
    return QSettings().value(kShowKey, true).toBool();
}

void WelcomeDialog::fillProjects()
{
    m_projects->clear();
    const QString language = LocalizedText::supported(UiLanguage::effective());
    const QFileInfoList files = QDir(UserFolders::projectsDir())
                                    .entryInfoList({QStringLiteral("*.") + QLatin1String(Project::fileSuffix)},
                                                   QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : files) {
        // By the project's name in the user's language, else the file's.
        const std::optional<Project> project = Project::loadFromFile(file.absoluteFilePath(), nullptr, language);
        const QString name = project ? project->name.text(language) : QString();
        auto *item = new QListWidgetItem(QApplication::windowIcon(),
                                         name.isEmpty() ? file.completeBaseName() : name, m_projects);
        item->setData(Qt::UserRole, file.absoluteFilePath());
        item->setToolTip(QDir::toNativeSeparators(file.absoluteFilePath()));
    }
    if (files.isEmpty()) {
        auto *item = new QListWidgetItem(tr("No projects yet"), m_projects);
        item->setFlags(Qt::NoItemFlags);
    }
}

void WelcomeDialog::fillDatabases()
{
    m_databases->clear();
    const QFileInfoList files = QDir(UserFolders::databasesDir())
                                    .entryInfoList({QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)},
                                                   QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : files) {
        // As Switch Database names it: the name given in Database Settings.
        const QString label = SqliteGameDatabase::readProperties(file.absoluteFilePath())
                                  .label(UiLanguage::effective(), file.absoluteFilePath());
        auto *item = new QListWidgetItem(SymbolicIcons::icon(QStringLiteral("pragma-database")), label, m_databases);
        item->setData(Qt::UserRole, file.absoluteFilePath());
        item->setToolTip(QDir::toNativeSeparators(file.absoluteFilePath()));
    }
    if (files.isEmpty()) {
        auto *item = new QListWidgetItem(tr("No databases yet"), m_databases);
        item->setFlags(Qt::NoItemFlags);
    }
}

void WelcomeDialog::choose(QListWidgetItem *item, bool project)
{
    const QString path = item ? item->data(Qt::UserRole).toString() : QString();
    if (path.isEmpty())
        return;
    // Closed first: what opens may ask questions (save the project?) of its own.
    accept();
    if (project)
        Q_EMIT projectChosen(path);
    else
        Q_EMIT databaseChosen(path);
}
