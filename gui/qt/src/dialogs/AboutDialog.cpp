#include "AboutDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kLogoSize = 64;

/// A club that supports Pragma Chess. Add the next ones here, with the logo
/// in resources/credits (listed in CMakeLists.txt).
struct Supporter {
    const char *name;
    const char *logo;
    const char *url;
};

const Supporter kSupporters[] = {
    {"ASD Circolo del Re – Castelvetrano Scacchi", ":/credits/circolo-del-re.png", "https://circolodelre.it"},
};

QLabel *logoLabel(const QPixmap &pixmap)
{
    auto *label = new QLabel;
    label->setPixmap(pixmap.scaled(kLogoSize, kLogoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->setFixedSize(kLogoSize, kLogoSize);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QLabel *textLabel(const QString &html)
{
    auto *label = new QLabel(html);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    return label;
}

QLabel *sectionTitle(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QFrame *rule()
{
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setEnabled(false);
    return line;
}

} // namespace

AboutDialog::AboutDialog(const QString &version, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Pragma Chess"));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // What it is.
    auto *header = new QGridLayout;
    header->setHorizontalSpacing(16);
    header->addWidget(logoLabel(QApplication::windowIcon().pixmap(kLogoSize, kLogoSize)), 0, 0, Qt::AlignTop);
    header->addWidget(textLabel(tr("<h2>Pragma Chess %1</h2>"
                                   "<p>An open source chess database for studying, training and playing, "
                                   "built to stay light and simple and to do what is useful.</p>"
                                   "<p>Licensed under the MIT License.</p>").arg(version)),
                      0, 1);
    header->setColumnStretch(1, 1);
    layout->addLayout(header);

    // Who supports it.
    layout->addWidget(rule());
    layout->addWidget(sectionTitle(tr("Supported by")));
    auto *supporters = new QGridLayout;
    supporters->setHorizontalSpacing(16);
    int row = 0;
    for (const Supporter &supporter : kSupporters) {
        supporters->addWidget(logoLabel(QPixmap(QString::fromLatin1(supporter.logo))), row, 0);
        const QString url = QString::fromLatin1(supporter.url);
        supporters->addWidget(textLabel(QStringLiteral("<b>%1</b><br><a href=\"%2\">%3</a>")
                                            .arg(QString::fromUtf8(supporter.name).toHtmlEscaped(), url,
                                                 QString(url).remove(QStringLiteral("https://")))),
                              row, 1);
        ++row;
    }
    supporters->setColumnStretch(1, 1);
    layout->addLayout(supporters);

    // What it is built with.
    layout->addWidget(rule());
    layout->addWidget(sectionTitle(tr("Built with")));
    auto *built = new QGridLayout;
    built->setHorizontalSpacing(16);
    built->addWidget(logoLabel(QPixmap(QStringLiteral(":/qt-project.org/qmessagebox/images/qtlogo-64.png"))), 0, 0,
                     Qt::AlignTop);
    built->addWidget(textLabel(tr("<b>Qt %1</b><br>The toolkit of the desktop client, used under the "
                                  "GNU Lesser General Public License version 3. "
                                  "<a href=\"https://www.qt.io\">qt.io</a>").arg(QString::fromLatin1(qVersion()))),
                     0, 1);
    auto *aboutQt = new QPushButton(tr("About &Qt…"));
    aboutQt->setToolTip(tr("Qt's own notice, with its licenses"));
    connect(aboutQt, &QPushButton::clicked, qApp, &QApplication::aboutQt);
    built->addWidget(aboutQt, 0, 2, Qt::AlignTop);
    built->addWidget(textLabel(tr("<b>Stockfish</b>, the chess engine that comes with Pragma Chess (GPL version 3, a "
                                  "program of its own) · the <b>Good Companion</b> chess pieces · the <b>SkakNew</b> "
                                  "figurines of the moves (LPPL) · the opening names of <b>lichess.org</b> (CC0).")),
                     1, 1, 1, 2);
    built->setColumnStretch(1, 1);
    layout->addLayout(built);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    setMinimumWidth(520);
}
