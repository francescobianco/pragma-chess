#pragma once

#include <QStringList>
#include <QToolButton>

/// A button whose icon sits in a section of its own on the left, set off by
/// a thin line, and whose text is centred in the rest. Its size is fixed to
/// fit the widest of the texts it will show (setTexts), so it does not jump
/// when its action changes name: Analyze and Stop in the Engine panel.
class SectionButton : public QToolButton {
    Q_OBJECT

public:
    explicit SectionButton(QWidget *parent = nullptr);

    /// Every text the button may show; it takes the size of the widest.
    void setTexts(const QStringList &texts);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int sectionWidth() const;

    QStringList m_texts;
};
