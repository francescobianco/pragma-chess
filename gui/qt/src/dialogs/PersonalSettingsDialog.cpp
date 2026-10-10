#include "PersonalSettingsDialog.h"

#include "widgets/BoardTheme.h"
#include "widgets/HelpButton.h"
#include "widgets/PieceRenderer.h"

#include <QClipboard>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kPreviewSquare = 40;

/// Two squares of the style, a white knight on the light one and a black
/// knight on the dark one: the colours and the pieces at a glance.
QIcon themePreview(const BoardTheme &theme, qreal devicePixelRatio)
{
    QPixmap pixmap(QSize(2 * kPreviewSquare, kPreviewSquare) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF light(0, 0, kPreviewSquare, kPreviewSquare);
    const QRectF dark = light.translated(kPreviewSquare, 0);
    const Piece blackKnight{PieceType::Knight, Side::Black};
    theme.paintSquares(painter, light.united(dark), {dark}, {BoardTheme::PlacedPiece{dark, blackKnight}},
                       devicePixelRatio);
    PieceRenderer::paint(painter, Piece{PieceType::Knight, Side::White}, light, devicePixelRatio, theme.pieceStyle());
    PieceRenderer::paint(painter, blackKnight, dark, devicePixelRatio, theme.pieceStyle());
    return QIcon(pixmap);
}

} // namespace

PersonalSettingsDialog::PersonalSettingsDialog(const PersonalSettings &settings, const QString &lobbyKey,
                                               QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(settings.name, this))
    , m_birthYear(new QSpinBox(this))
    , m_fideId(new QLineEdit(settings.fideId, this))
    , m_boardTheme(new QComboBox(this))
{
    setWindowTitle(tr("Personal Settings"));
    setMinimumWidth(460);

    m_name->setPlaceholderText(tr("As it should appear in your games, e.g. Rossi, Mario"));
    m_name->setClearButtonEnabled(true);
    // The lowest value shows as a dash: no year given.
    m_birthYear->setRange(1899, QDate::currentDate().year());
    m_birthYear->setSpecialValueText(QStringLiteral("—"));
    m_birthYear->setValue(settings.birthYear > 0 ? settings.birthYear : m_birthYear->minimum());
    m_fideId->setPlaceholderText(tr("Digits only, e.g. 896489"));
    m_fideId->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,12}")), m_fideId));

    m_boardTheme->setIconSize(QSize(2 * kPreviewSquare, kPreviewSquare));
    for (const BoardTheme &theme : BoardTheme::all())
        m_boardTheme->addItem(themePreview(theme, devicePixelRatioF()), theme.name, theme.id);
    m_boardTheme->setCurrentIndex(m_boardTheme->findData(BoardTheme::byId(settings.boardTheme).id));

    // What the window is for, before the fields.
    auto *intro = new QLabel(tr("Who you are and how you like your board: your name, year of birth and FIDE ID, and "
                                "the style of the board. They are kept in your Pragma folder, and Sync carries them "
                                "to your other computers — all but the lobby key, which stays on this one."),
                             this);
    intro->setWordWrap(true);

    auto *form = new QFormLayout;
    form->addRow(tr("My &name:"),
                 HelpButton::beside(m_name,
                                    tr("<p>Your name as it should appear in your games: it goes on your side of new "
                                       "games and training games.</p>"
                                       "<p>When the open database already knows you — a player marked as Me with "
                                       "Who Is This? — that one wins. Until you give a name, Pragma Chess gives you "
                                       "one, a champion's with three digits.</p>"),
                                    this));
    form->addRow(tr("Year of &birth:"),
                 HelpButton::beside(m_birthYear,
                                    tr("<p>The year you were born, kept with your settings.</p>"
                                       "<p>Optional: leave the dash if you would rather not give it.</p>"),
                                    this));
    form->addRow(tr("&FIDE ID:"),
                 HelpButton::beside(m_fideId,
                                    tr("<p>Your number at FIDE, the world chess federation: the digits of your FIDE "
                                       "profile.</p><p>Optional.</p>"),
                                    this));
    // The board's style apart, under a line, and larger: it is seen, not read.
    auto *styleLine = new QFrame(this);
    styleLine->setFrameShape(QFrame::HLine);
    styleLine->setFrameShadow(QFrame::Sunken);
    form->addRow(styleLine);
    m_boardTheme->setMinimumHeight(kPreviewSquare + 12);
    form->addRow(tr("Board &style:"),
                 HelpButton::beside(m_boardTheme,
                                    tr("<p>The colours of the squares and the pieces, together: Pragma Classic, Lichess "
                                       "Alpha or Classic Book.</p>"
                                       "<p>Every board changes as soon as you press OK, and on your other computers "
                                       "with Sync.</p>"),
                                    this));

    // The lobby key: who the user is in the lobby, kept on this computer only.
    if (!lobbyKey.isEmpty()) {
        m_originalLobbyKey = lobbyKey;
        auto *lobby = new QWidget(this);
        auto *keyRow = new QHBoxLayout(lobby);
        keyRow->setContentsMargins(0, 0, 0, 0);
        m_lobbyKey = new QLineEdit(lobbyKey, lobby);
        m_lobbyKey->setEchoMode(QLineEdit::Password); // A secret: shown only when asked.
        m_lobbyKey->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_lobbyKey->setValidator(
            new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9a-fA-F]{0,64}")), m_lobbyKey));
        auto *show = new QToolButton(lobby);
        show->setText(tr("Show"));
        show->setCheckable(true);
        connect(show, &QToolButton::toggled, this, [this, show](bool on) {
            m_lobbyKey->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
            show->setText(on ? tr("Hide") : tr("Show"));
        });
        auto *copy = new QToolButton(lobby);
        copy->setText(tr("Copy"));
        connect(copy, &QToolButton::clicked, this, [this] { QGuiApplication::clipboard()->setText(m_lobbyKey->text()); });
        keyRow->addWidget(m_lobbyKey, 1);
        keyRow->addWidget(show);
        keyRow->addWidget(copy);
        keyRow->addWidget(new HelpButton(tr("<p>This key is who you are in the lobby: it signs your moves.</p>"
                                            "<p>For security it is not synced with your other settings and stays on "
                                            "this computer only. To play as yourself from another computer, copy it "
                                            "here and paste it into the same field there: you have to carry it "
                                            "yourself.</p><p>Anyone who has it can play as you, so keep it to "
                                            "yourself.</p>"),
                                         lobby),
                          0, Qt::AlignVCenter);
        // The lobby apart, under a line, in the same columns: it is no setting
        // of yours, it is who you are there.
        auto *line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        form->addRow(line);
        auto *label = new QLabel(tr("&Lobby key:"), this);
        label->setBuddy(m_lobbyKey); // Its letter takes the focus there.
        form->addRow(label, lobby);
        m_lobbyKey->setAccessibleName(tr("Lobby key"));
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PersonalSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addSpacing(6);
    layout->addLayout(form);
    layout->addSpacing(18); // Room before the window's buttons.
    layout->addStretch();
    layout->addWidget(buttons);
}

QString PersonalSettingsDialog::lobbyKey() const
{
    return m_lobbyKey ? m_lobbyKey->text().trimmed().toLower() : QString();
}

void PersonalSettingsDialog::accept()
{
    if (m_lobbyKey && lobbyKey() != m_originalLobbyKey) {
        if (lobbyKey().size() != 64) {
            QMessageBox::warning(this, tr("Lobby Key"), tr("A lobby key is 64 hexadecimal digits: paste it whole."));
            return;
        }
        // The key of this computer goes: unless it was copied, who it was in the lobby is gone with it.
        const auto answer = QMessageBox::question(
            this, tr("Lobby Key"),
            tr("This computer will be the player of the new key in the lobby. Its present key is replaced: if you "
               "have not copied it, you can no longer play as that player. Replace it?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }
    QDialog::accept();
}

PersonalSettings PersonalSettingsDialog::settings() const
{
    PersonalSettings settings;
    settings.name = m_name->text().trimmed();
    settings.birthYear = m_birthYear->value() > m_birthYear->minimum() ? m_birthYear->value() : 0;
    settings.fideId = m_fideId->text().trimmed();
    // Written even when it is the default: the choice is meant for every synced computer.
    settings.boardTheme = m_boardTheme->currentData().toString();
    return settings;
}
