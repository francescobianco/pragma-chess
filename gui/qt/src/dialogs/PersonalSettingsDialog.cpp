#include "PersonalSettingsDialog.h"

#include "widgets/BoardTheme.h"
#include "widgets/PieceRenderer.h"

#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

constexpr int kPreviewSquare = 28;

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
    painter.fillRect(light, theme.lightSquare);
    painter.fillRect(dark, theme.darkSquare);
    PieceRenderer::paint(painter, Piece{PieceType::Knight, Side::White}, light, devicePixelRatio, theme.pieceSet);
    PieceRenderer::paint(painter, Piece{PieceType::Knight, Side::Black}, dark, devicePixelRatio, theme.pieceSet);
    return QIcon(pixmap);
}

} // namespace

PersonalSettingsDialog::PersonalSettingsDialog(const PersonalSettings &settings, QWidget *parent)
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

    auto *form = new QFormLayout;
    form->addRow(tr("My &name:"), m_name);
    form->addRow(tr("Year of &birth:"), m_birthYear);
    form->addRow(tr("&FIDE ID:"), m_fideId);
    form->addRow(tr("Board &style:"), m_boardTheme);

    auto *note = new QLabel(tr("Your name goes on your side of new games and training games, unless the open "
                               "database already knows you: a player marked as Me with Who Is This? wins. These "
                               "settings are kept in .pragma-chess.conf in your Pragma folder, which Sync carries "
                               "to your other computers."),
                            this);
    note->setWordWrap(true);
    note->setEnabled(false); // Greyed: a remark, not a setting.

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addStretch();
    layout->addWidget(buttons);
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
