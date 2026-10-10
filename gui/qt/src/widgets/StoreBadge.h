#pragma once

#include <QAbstractButton>
#include <QUrl>

/// A store badge in the classic shape — a dark rounded tile, a small line
/// over the store's name — drawn in code, without the stores' own logos
/// (theirs only for apps they list). With a URL it opens it; without, it is
/// "coming soon": faint, not clickable.
class StoreBadge : public QAbstractButton {
    Q_OBJECT

public:
    /// `caption` the small line ("Download for"), `name` the store's.
    StoreBadge(const QString &caption, const QString &name, const QUrl &url, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_caption;
    QString m_name;
    QUrl m_url;
};
