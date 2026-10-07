#include "DrawersDialog.h"

#include "widgets/FigurineFont.h"
#include "widgets/PaddedItemDelegate.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

DrawersDialog::DrawersDialog(const Drawers &drawers, QWidget *parent)
    : QDialog(parent)
    , m_drawers(drawers)
{
    setWindowTitle(tr("Drawers"));
    resize(720, 460);

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Each drawer keeps something under a name — moves, a variation, a position, "
                                "a note — to take it out again when you need it. The drawers are yours: "
                                "they go with you to every synced computer."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *columns = new QHBoxLayout;
    auto *left = new QVBoxLayout;
    m_list = new QListWidget(this);
    m_list->setMaximumWidth(220);
    m_list->setItemDelegate(new PaddedItemDelegate(CellPadding::vertical, CellPadding::horizontal, m_list));
    left->addWidget(m_list, 1);
    auto *listButtons = new QHBoxLayout;
    auto *addButton = new QPushButton(tr("&New Drawer"), this);
    m_deleteButton = new QPushButton(tr("&Delete"), this);
    listButtons->addWidget(addButton);
    listButtons->addWidget(m_deleteButton);
    left->addLayout(listButtons);
    columns->addLayout(left);

    auto *form = new QFormLayout;
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("The name you will call it by"));
    form->addRow(tr("&Name:"), m_name);
    m_content = new QPlainTextEdit(this);
    m_content->setFont(FigurineFont::apply(m_content->font())); // Moves written with figurines read as in the move list.
    m_content->setPlaceholderText(tr("Moves, a variation, a FEN position, a note…"));
    form->addRow(tr("&Content:"), m_content);
    columns->addLayout(form, 1);
    layout->addLayout(columns, 1);

    m_problem = new QLabel(this);
    m_problem->setWordWrap(true);
    m_problem->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_problem);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DrawersDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, &DrawersDialog::showDrawer);
    connect(addButton, &QPushButton::clicked, this, &DrawersDialog::addDrawer);
    connect(m_deleteButton, &QPushButton::clicked, this, &DrawersDialog::deleteDrawer);
    connect(m_name, &QLineEdit::textEdited, this, [this](const QString &name) {
        if (m_current < 0)
            return;
        m_drawers.list[m_current].name = name;
        m_list->item(m_current)->setText(name.trimmed().isEmpty() ? tr("(no name)") : name.trimmed());
        m_problem->clear();
    });
    connect(m_content, &QPlainTextEdit::textChanged, this, [this] {
        if (m_current >= 0)
            m_drawers.list[m_current].content = m_content->toPlainText();
    });

    fillList(m_drawers.list.isEmpty() ? -1 : 0);
}

void DrawersDialog::fillList(int current)
{
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const Drawer &drawer : std::as_const(m_drawers.list))
            m_list->addItem(drawer.name.trimmed().isEmpty() ? tr("(no name)") : drawer.name.trimmed());
    }
    m_list->setCurrentRow(current);
    showDrawer(current);
}

void DrawersDialog::showDrawer(int row)
{
    m_current = row >= 0 && row < m_drawers.list.size() ? row : -1;
    const bool shown = m_current >= 0;
    {
        const QSignalBlocker nameBlocker(m_name);
        const QSignalBlocker contentBlocker(m_content);
        m_name->setText(shown ? m_drawers.list.at(m_current).name : QString());
        m_content->setPlainText(shown ? m_drawers.list.at(m_current).content : QString());
    }
    m_name->setEnabled(shown);
    m_content->setEnabled(shown);
    m_deleteButton->setEnabled(shown);
    if (!shown && m_drawers.list.isEmpty())
        m_problem->setText(tr("No drawers yet: New Drawer makes the first."));
    else
        m_problem->clear();
}

void DrawersDialog::addDrawer()
{
    m_drawers.list << Drawer{};
    fillList(int(m_drawers.list.size()) - 1);
    m_name->setFocus();
}

void DrawersDialog::deleteDrawer()
{
    if (m_current < 0)
        return;
    m_drawers.list.removeAt(m_current);
    fillList(qMin(m_current, int(m_drawers.list.size()) - 1));
}

void DrawersDialog::accept()
{
    const QString problem = m_drawers.problem();
    if (!problem.isEmpty()) {
        m_problem->setText(problem);
        return;
    }
    QDialog::accept();
}
