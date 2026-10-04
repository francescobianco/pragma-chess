#include "PositionSetupDialog.h"

#include "widgets/PieceRenderer.h"
#include "widgets/PositionEditorWidget.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kPaletteIcon = 36;

QIcon pieceIcon(Piece piece, qreal ratio)
{
    QPixmap pixmap(QSize(kPaletteIcon, kPaletteIcon) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    PieceRenderer::paint(painter, piece, QRectF(0, 0, kPaletteIcon, kPaletteIcon), ratio);
    return QIcon(pixmap);
}

} // namespace

PositionSetupDialog::PositionSetupDialog(const QString &fen, bool flipped, QWidget *parent)
    : QDialog(parent)
    , m_board(new PositionEditorWidget(this))
    , m_palette(new QButtonGroup(this))
{
    setWindowTitle(tr("Set Up Position"));
    m_board->setSetup(PositionSetup::fromFen(fen).value_or(PositionSetup::startingPosition()));
    m_board->setFlipped(flipped);

    // The pieces to put down, White's row above Black's, and the eraser.
    auto *palette = new QGridLayout;
    palette->setSpacing(2);
    const PieceType types[] = {PieceType::King, PieceType::Queen, PieceType::Rook,
                               PieceType::Bishop, PieceType::Knight, PieceType::Pawn};
    const qreal ratio = devicePixelRatioF();
    int id = 0;
    for (const Side side : {Side::White, Side::Black}) {
        for (int column = 0; column < 6; ++column) {
            auto *button = new QToolButton(this);
            button->setCheckable(true);
            button->setAutoRaise(true);
            button->setIconSize(QSize(kPaletteIcon, kPaletteIcon));
            button->setIcon(pieceIcon({types[column], side}, ratio));
            m_palette->addButton(button, id++);
            palette->addWidget(button, int(side), column);
        }
    }
    auto *eraser = new QToolButton(this);
    eraser->setCheckable(true);
    eraser->setAutoRaise(true);
    eraser->setText(tr("Remove"));
    eraser->setToolTip(tr("A click takes the piece off the square. A right click does it too."));
    m_palette->addButton(eraser, id);
    palette->addWidget(eraser, 2, 0, 1, 6);
    m_palette->button(5)->setChecked(true); // White pawn: the piece put down most.
    connect(m_palette, &QButtonGroup::idClicked, this, [this, types](int clicked) {
        m_board->setBrush(clicked >= 12 ? Piece() : Piece{types[clicked % 6], clicked < 6 ? Side::White : Side::Black});
    });

    auto *toMove = new QGroupBox(tr("Side to move"), this);
    m_whiteToMove = new QRadioButton(tr("&White"), toMove);
    m_blackToMove = new QRadioButton(tr("Blac&k"), toMove);
    auto *toMoveLayout = new QHBoxLayout(toMove);
    toMoveLayout->addWidget(m_whiteToMove);
    toMoveLayout->addWidget(m_blackToMove);

    auto *castling = new QGroupBox(tr("Castling"), this);
    m_whiteKingSide = new QCheckBox(tr("White O-O"), castling);
    m_whiteQueenSide = new QCheckBox(tr("White O-O-O"), castling);
    m_blackKingSide = new QCheckBox(tr("Black O-O"), castling);
    m_blackQueenSide = new QCheckBox(tr("Black O-O-O"), castling);
    auto *castlingLayout = new QGridLayout(castling);
    castlingLayout->addWidget(m_whiteKingSide, 0, 0);
    castlingLayout->addWidget(m_whiteQueenSide, 0, 1);
    castlingLayout->addWidget(m_blackKingSide, 1, 0);
    castlingLayout->addWidget(m_blackQueenSide, 1, 1);

    m_enPassant = new QComboBox(this);
    m_moveNumber = new QSpinBox(this);
    m_moveNumber->setRange(1, 999);
    auto *details = new QFormLayout;
    details->addRow(tr("&En passant:"), m_enPassant);
    details->addRow(tr("&Move number:"), m_moveNumber);

    auto *startingPosition = new QPushButton(tr("&Starting Position"), this);
    auto *clear = new QPushButton(tr("&Clear Board"), this);
    auto *flip = new QPushButton(tr("&Flip Board"), this);
    connect(startingPosition, &QPushButton::clicked, this, [this] {
        m_board->setSetup(PositionSetup::startingPosition());
        showSetup();
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        PositionSetup empty = PositionSetup::empty();
        empty.sideToMove = m_board->setup().sideToMove;
        m_board->setSetup(empty);
        showSetup();
    });
    connect(flip, &QPushButton::clicked, this, [this] { m_board->setFlipped(!m_board->isFlipped()); });
    auto *actions = new QHBoxLayout;
    actions->addWidget(startingPosition);
    actions->addWidget(clear);
    actions->addWidget(flip);

    auto *side = new QVBoxLayout;
    side->addLayout(palette);
    side->addWidget(toMove);
    side->addWidget(castling);
    side->addLayout(details);
    side->addStretch();
    side->addLayout(actions);

    auto *top = new QHBoxLayout;
    top->addWidget(m_board, 1);
    top->addLayout(side);

    m_fen = new QLineEdit(this);
    m_fen->setToolTip(tr("Type or paste a FEN to set up its position"));
    auto *fenRow = new QFormLayout;
    fenRow->addRow(tr("FE&N:"), m_fen);
    m_problem = new QLabel(this);
    m_problem->setWordWrap(true);
    m_problem->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // Its room is there from the start, two lines of it: a message coming and
    // going must not shrink and grow the board above it.
    m_problem->setFixedHeight(2 * m_problem->fontMetrics().lineSpacing());
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top, 1);
    layout->addLayout(fenRow);
    layout->addWidget(m_problem);
    layout->addWidget(m_buttons);

    connect(m_board, &PositionEditorWidget::changed, this, [this] { showSetup(); });
    for (QAbstractButton *button : std::initializer_list<QAbstractButton *>{
             m_whiteToMove, m_blackToMove, m_whiteKingSide, m_whiteQueenSide, m_blackKingSide, m_blackQueenSide})
        connect(button, &QAbstractButton::toggled, this, &PositionSetupDialog::readControls);
    connect(m_enPassant, &QComboBox::currentIndexChanged, this, &PositionSetupDialog::readControls);
    connect(m_moveNumber, &QSpinBox::valueChanged, this, &PositionSetupDialog::readControls);
    connect(m_fen, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (const std::optional<PositionSetup> setup = PositionSetup::fromFen(text.trimmed())) {
            m_board->setSetup(*setup);
            showSetup(false); // The text stays as typed.
        }
    });
    showSetup();
}

QString PositionSetupDialog::fen() const
{
    return m_board->setup().fen();
}

void PositionSetupDialog::showSetup(bool fenToo)
{
    m_updating = true;
    const PositionSetup &setup = m_board->setup();
    m_whiteToMove->setChecked(setup.sideToMove == Side::White);
    m_blackToMove->setChecked(setup.sideToMove == Side::Black);
    // A right the pieces do not allow cannot be ticked.
    const auto showRight = [&setup](QCheckBox *box, bool right, Side side, bool kingSide) {
        const bool possible = setup.canCastle(side, kingSide);
        box->setEnabled(possible);
        box->setChecked(possible && right);
    };
    showRight(m_whiteKingSide, setup.whiteKingSide, Side::White, true);
    showRight(m_whiteQueenSide, setup.whiteQueenSide, Side::White, false);
    showRight(m_blackKingSide, setup.blackKingSide, Side::Black, true);
    showRight(m_blackQueenSide, setup.blackQueenSide, Side::Black, false);
    m_enPassant->clear();
    m_enPassant->addItem(tr("None"), -1);
    for (const int square : setup.enPassantSquares())
        m_enPassant->addItem(BoardState::squareName(square), square);
    m_enPassant->setCurrentIndex(qMax(0, m_enPassant->findData(setup.enPassant)));
    m_enPassant->setEnabled(m_enPassant->count() > 1);
    m_moveNumber->setValue(setup.fullMove);
    if (fenToo)
        m_fen->setText(setup.fen());
    const QString problem = setup.problem();
    m_problem->setText(problem);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(problem.isEmpty());
    m_updating = false;
}

void PositionSetupDialog::readControls()
{
    if (m_updating)
        return;
    PositionSetup setup = m_board->setup();
    setup.sideToMove = m_blackToMove->isChecked() ? Side::Black : Side::White;
    // A right that was not possible stays as it was, for when it becomes possible.
    if (m_whiteKingSide->isEnabled())
        setup.whiteKingSide = m_whiteKingSide->isChecked();
    if (m_whiteQueenSide->isEnabled())
        setup.whiteQueenSide = m_whiteQueenSide->isChecked();
    if (m_blackKingSide->isEnabled())
        setup.blackKingSide = m_blackKingSide->isChecked();
    if (m_blackQueenSide->isEnabled())
        setup.blackQueenSide = m_blackQueenSide->isChecked();
    setup.enPassant = m_enPassant->currentData().toInt();
    setup.fullMove = m_moveNumber->value();
    m_board->setSetup(setup);
    showSetup();
}
