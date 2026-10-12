#pragma once

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// A balloon pointing at a control of the window to say what it does, the
/// first time the program thinks it would help (FeatureDiscovery): rich
/// text, "Don't show this again" and OK, in its shadow. A child of the
/// top-level window, over everything in it — no window of its own, which a
/// Wayland compositor places where it likes —, above the control with its
/// tip down, over the board where the eyes are (under it where there is no
/// room above), and following the control when the window is laid out
/// again. Closed by OK or from outside (`close()`), it says whether the
/// user asked not to see it again.
class DiscoveryBalloon : public QWidget {
    Q_OBJECT

public:
    DiscoveryBalloon(const QString &text, QWidget *anchor);

    bool dontShowAgain() const;

Q_SIGNALS:
    /// Once, when the balloon closes: `dontShowAgain` what its box says.
    void closed(bool dontShowAgain);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void place();

    QPointer<QWidget> m_anchor;
    QLabel *m_label;
    QCheckBox *m_dontShow;
    QPushButton *m_ok;
    QVBoxLayout *m_layout;
    bool m_below = true;
    int m_tipX = 0;
    bool m_closedSaid = false;
};
