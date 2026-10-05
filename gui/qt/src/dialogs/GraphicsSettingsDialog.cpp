#include "GraphicsSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

GraphicsSettingsDialog::GraphicsSettingsDialog(const GraphicsSettings &settings, bool showCoordinates, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Graphics Settings"));

    auto *layout = new QVBoxLayout(this);

    m_appearance = new QComboBox(this);
    m_appearance->addItem(tr("Follow the System"), int(AppearanceMode::System));
    m_appearance->addItem(tr("Light"), int(AppearanceMode::Light));
    m_appearance->addItem(tr("Dark"), int(AppearanceMode::Dark));
    m_appearance->setCurrentIndex(m_appearance->findData(int(settings.appearance)));
    auto *form = new QFormLayout;
    form->addRow(tr("&Appearance:"), m_appearance);
    layout->addLayout(form);

    auto *captured = new QGroupBox(tr("Captured pieces"), this);
    auto *capturedLayout = new QVBoxLayout(captured);
    m_besideBoard = new QRadioButton(tr("&Right of the board, next to each player"), captured);
    m_belowBoard = new QRadioButton(tr("&Below the board, on the left"), captured);
    capturedLayout->addWidget(m_besideBoard);
    capturedLayout->addWidget(m_belowBoard);
    (settings.capturedPieces == CapturedPiecesPlacement::BelowBoard ? m_belowBoard : m_besideBoard)->setChecked(true);
    layout->addWidget(captured);

    m_showTurn = new QCheckBox(tr("Show whose &turn it is"), this);
    m_showTurn->setToolTip(tr("A white or black dot at the right of the board, on the side of the player to move"));
    m_showTurn->setChecked(settings.showTurn);
    layout->addWidget(m_showTurn);

    m_showCoordinates = new QCheckBox(tr("Show &coordinates"), this);
    m_showCoordinates->setChecked(showCoordinates);
    layout->addWidget(m_showCoordinates);

    auto *note = new QLabel(tr("These settings are kept on this computer only: each computer can match its own "
                               "desktop. The board style is in Personal Settings, and travels with Sync."),
                            this);
    note->setWordWrap(true);
    note->setEnabled(false); // Greyed: a remark, not a setting.
    layout->addWidget(note);

    layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

GraphicsSettings GraphicsSettingsDialog::settings() const
{
    GraphicsSettings settings;
    settings.capturedPieces = m_belowBoard->isChecked() ? CapturedPiecesPlacement::BelowBoard
                                                        : CapturedPiecesPlacement::BesideBoard;
    settings.showTurn = m_showTurn->isChecked();
    settings.appearance = AppearanceMode(m_appearance->currentData().toInt());
    return settings;
}

bool GraphicsSettingsDialog::showCoordinates() const
{
    return m_showCoordinates->isChecked();
}
