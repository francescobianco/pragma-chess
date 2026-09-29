#include "BoardSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

BoardSettingsDialog::BoardSettingsDialog(const BoardSettings &settings, bool showCoordinates, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Board Settings"));

    auto *layout = new QVBoxLayout(this);

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

    layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

BoardSettings BoardSettingsDialog::settings() const
{
    BoardSettings settings;
    settings.capturedPieces = m_belowBoard->isChecked() ? CapturedPiecesPlacement::BelowBoard
                                                        : CapturedPiecesPlacement::BesideBoard;
    settings.showTurn = m_showTurn->isChecked();
    return settings;
}

bool BoardSettingsDialog::showCoordinates() const
{
    return m_showCoordinates->isChecked();
}
