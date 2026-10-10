#pragma once

#include <QPointer>
#include <QToolButton>

class QTimer;

/// A small "?" at the end of a field: hovered (or clicked, or focused and
/// Space), it shows its text in a balloon whose tip comes out of the
/// button — paragraphs at a width that reads, not one long line as a
/// tooltip would. The text is rich text; paragraphs are <p>…</p>.
class HelpButton : public QToolButton {
    Q_OBJECT

public:
    explicit HelpButton(const QString &text, QWidget *parent = nullptr);
    ~HelpButton() override;

    void setHelpText(const QString &text);
    /// Shows the balloon as hovering does; the balloon shown, or null.
    void showHelp() { showBubble(); }
    QWidget *bubble() const { return m_bubble; }

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void showBubble();
    void hideBubble();

    QString m_text;
    QTimer *m_delay;
    QPointer<QWidget> m_bubble;
};
