#include "GameInfoDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QSpinBox *eloSpinBox(int value)
{
    auto *spin = new QSpinBox;
    spin->setRange(0, 3500);
    spin->setSpecialValueText(QStringLiteral("–")); // 0 = unknown
    spin->setValue(value);
    spin->setAccessibleName(QObject::tr("Elo"));
    return spin;
}

QString unknownToEmpty(const QString &value)
{
    return value.trimmed() == QLatin1String("?") ? QString() : value;
}

QLabel *sectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // namespace

GameInfoDialog::GameInfoDialog(const GameRecord &game, QWidget *parent)
    : QDialog(parent)
    , m_game(game)
    , m_white(new QLineEdit(unknownToEmpty(game.white)))
    , m_whiteElo(eloSpinBox(game.whiteElo))
    , m_black(new QLineEdit(unknownToEmpty(game.black)))
    , m_blackElo(eloSpinBox(game.blackElo))
    , m_event(new QLineEdit(unknownToEmpty(game.event)))
    , m_site(new QLineEdit(unknownToEmpty(game.site)))
    , m_date(new QLineEdit(game.date))
    , m_round(new QLineEdit(unknownToEmpty(game.round)))
    , m_result(new QComboBox)
    , m_eco(new QLineEdit(game.eco))
{
    setWindowTitle(tr("Game Information"));

    for (QLineEdit *name : {m_white, m_black})
        name->setPlaceholderText(tr("Surname, Name"));
    m_white->setAccessibleName(tr("White"));
    m_black->setAccessibleName(tr("Black"));
    m_event->setPlaceholderText(tr("Tournament or match"));
    m_site->setPlaceholderText(tr("City COUNTRY"));

    // PGN dates: YYYY.MM.DD with ?? for unknown parts.
    m_date->setPlaceholderText(tr("YYYY.MM.DD"));
    m_date->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral(R"((\d{4}|\?{4})(\.(\d{2}|\?{2})(\.(\d{2}|\?{2}))?)?)")), this));
    m_eco->setPlaceholderText(QStringLiteral("B90"));
    m_eco->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral(R"([A-Ea-e](\d{2}([a-z]|\d{2})?)?)")), this));
    m_eco->setMaximumWidth(fontMetrics().horizontalAdvance(QStringLiteral("B90a00")) * 2);

    m_result->addItem(tr("White wins (1-0)"), QStringLiteral("1-0"));
    m_result->addItem(tr("Black wins (0-1)"), QStringLiteral("0-1"));
    m_result->addItem(tr("Draw (½-½)"), QStringLiteral("1/2-1/2"));
    m_result->addItem(tr("Unknown or ongoing (*)"), QStringLiteral("*"));
    const int resultIndex = m_result->findData(game.result.isEmpty() ? QStringLiteral("*") : game.result);
    m_result->setCurrentIndex(resultIndex < 0 ? 3 : resultIndex);

    auto *players = new QGridLayout;
    players->setColumnStretch(1, 1);
    auto *eloHeader = new QLabel(tr("Elo"));
    eloHeader->setEnabled(false);
    players->addWidget(eloHeader, 0, 2, Qt::AlignHCenter | Qt::AlignBottom);
    players->addWidget(new QLabel(tr("White")), 1, 0);
    players->addWidget(m_white, 1, 1);
    players->addWidget(m_whiteElo, 1, 2);
    players->addWidget(new QLabel(tr("Black")), 2, 0);
    players->addWidget(m_black, 2, 1);
    players->addWidget(m_blackElo, 2, 2);

    auto *details = new QFormLayout;
    details->addRow(tr("Event"), m_event);
    details->addRow(tr("Site"), m_site);
    details->addRow(tr("Date"), m_date);
    details->addRow(tr("Round"), m_round);
    details->addRow(tr("Result"), m_result);
    details->addRow(tr("ECO"), m_eco);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(sectionLabel(tr("Players")));
    layout->addLayout(players);
    layout->addSpacing(12);
    layout->addWidget(sectionLabel(tr("Game")));
    layout->addLayout(details);
    layout->addSpacing(8);
    layout->addWidget(buttons);

    setMinimumWidth(460);
    m_white->setFocus();
}

GameRecord GameInfoDialog::game() const
{
    GameRecord game = m_game;
    game.white = m_white->text().simplified();
    game.black = m_black->text().simplified();
    game.whiteElo = m_whiteElo->value();
    game.blackElo = m_blackElo->value();
    game.event = m_event->text().simplified();
    game.site = m_site->text().simplified();
    game.date = m_date->text().trimmed();
    game.round = m_round->text().simplified();
    game.result = m_result->currentData().toString();
    // ECO codes are upper case, sub-codes lower case (e.g. "B90a").
    const QString eco = m_eco->text().trimmed();
    game.eco = eco.left(3).toUpper() + eco.mid(3).toLower();
    return game;
}
