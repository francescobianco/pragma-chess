#pragma once

#include <QWidgetAction>

/// A menu item made of a symbol and, in italics, a memo of what it means:
/// "!!  brilliant move". QMenu draws the text of an item in one font, so the
/// item is a widget, painted and behaving as the style's own items. Without a
/// symbol the memo is an ordinary text in the same column ("No Annotation").
class GlyphMenuAction : public QWidgetAction {
    Q_OBJECT

public:
    GlyphMenuAction(const QString &symbol, const QString &memo, QObject *parent = nullptr);

    const QString &symbol() const { return m_symbol; }
    const QString &memo() const { return m_memo; }

protected:
    QWidget *createWidget(QWidget *parent) override;

private:
    QString m_symbol;
    QString m_memo;
};
