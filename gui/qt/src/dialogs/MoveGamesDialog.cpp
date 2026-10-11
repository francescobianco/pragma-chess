#include "MoveGamesDialog.h"

#include "app/MoveGames.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

MoveGamesDialog::MoveGamesDialog(int count, const QList<Database> &databases, const QString &folder,
                                 bool openWhenDone, QWidget *parent)
    : QDialog(parent)
    , m_folder(folder)
{
    setWindowTitle(count > 1 ? tr("Move Games") : tr("Move Game"));

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(count > 1 ? tr("Move %n games to another database. They leave this one once they "
                                            "are there.", nullptr, count)
                                       : tr("Move the game to another database. It leaves this one once it is "
                                            "there."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout;
    m_target = new QComboBox(this);
    for (const Database &database : databases)
        m_target->addItem(database.label, database.path);
    m_target->insertSeparator(m_target->count());
    m_target->addItem(tr("New Database…"));
    // With no other database, the new one is the only choice.
    m_target->setCurrentIndex(databases.isEmpty() ? m_target->count() - 1 : 0);
    form->addRow(tr("&To:"), m_target);
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("Name of the new database"));
    m_nameLabel = new QLabel(tr("&Name:"), this);
    m_nameLabel->setBuddy(m_name);
    form->addRow(m_nameLabel, m_name);
    layout->addLayout(form);

    m_problem = new QLabel(this);
    m_problem->setWordWrap(true);
    QPalette palette = m_problem->palette();
    palette.setColor(QPalette::WindowText, QColor(0xc0, 0x1c, 0x28));
    m_problem->setPalette(palette);
    m_problem->hide();
    layout->addWidget(m_problem);

    m_open = new QCheckBox(tr("&Open the database when done"), this);
    m_open->setChecked(openWhenDone);
    layout->addSpacing(layout->spacing());
    layout->addWidget(m_open);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Move"));
    connect(buttons, &QDialogButtonBox::accepted, this, &MoveGamesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_target, &QComboBox::currentIndexChanged, this, &MoveGamesDialog::updateNewName);
    connect(m_name, &QLineEdit::textChanged, m_problem, &QWidget::hide);
    updateNewName();
    setMinimumWidth(420);
}

bool MoveGamesDialog::isNew() const
{
    return m_target->currentIndex() == m_target->count() - 1;
}

QString MoveGamesDialog::path() const
{
    return m_path;
}

bool MoveGamesDialog::openWhenDone() const
{
    return m_open->isChecked();
}

void MoveGamesDialog::updateNewName()
{
    // The name is asked only for a new database. Its row stays in place,
    // disabled otherwise: a row coming and going would change the window's
    // height, and squeeze the others in a window already sized.
    m_nameLabel->setEnabled(isNew());
    m_name->setEnabled(isNew());
    m_problem->hide();
    if (isNew())
        m_name->setFocus();
}

void MoveGamesDialog::accept()
{
    if (isNew()) {
        QString problem;
        m_path = MoveGames::newDatabasePath(m_folder, m_name->text(), &problem);
        if (m_path.isEmpty()) {
            m_problem->setText(problem);
            m_problem->show();
            adjustSize(); // The window grows by the message, nothing is squeezed.
            m_name->setFocus();
            return;
        }
    } else {
        m_path = m_target->currentData().toString();
    }
    QDialog::accept();
}
