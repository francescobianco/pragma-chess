#pragma once

#include <QList>
#include <QStatusBar>

class QFrame;
class QLabel;

/// A status bar whose text keeps clear of both window edges. On macOS the
/// window's rounded bottom corners cut into QStatusBar's own message, which
/// Qt draws at a fixed spot no margin moves; here the message is a label
/// inside the margins, like the permanent widgets on the right.
class PaddedStatusBar : public QStatusBar {
    Q_OBJECT

public:
    explicit PaddedStatusBar(QWidget *parent = nullptr);

    /// Adds a section on the right, after the others, with a thin vertical
    /// line between it and the visible section before it, so the texts do
    /// not read as one. A hidden section takes its line with it.
    void addSection(QWidget *section);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Shows the lines that stand between two visible sections.
    void updateSeparators();

    QLabel *m_message;
    QList<QWidget *> m_sections;
    /// The line before each section (none before the first).
    QList<QFrame *> m_separators;
};
