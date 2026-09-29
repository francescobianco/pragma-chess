#pragma once

#include <QStatusBar>

class QLabel;

/// A status bar whose text keeps clear of both window edges. On macOS the
/// window's rounded bottom corners cut into QStatusBar's own message, which
/// Qt draws at a fixed spot no margin moves; here the message is a label
/// inside the margins, like the permanent widgets on the right.
class PaddedStatusBar : public QStatusBar {
    Q_OBJECT

public:
    explicit PaddedStatusBar(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_message;
};
