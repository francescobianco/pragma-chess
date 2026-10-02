#include "HelpDialog.h"

#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

namespace {

constexpr int kTopicRole = Qt::UserRole;
constexpr int kSnippetRole = Qt::UserRole + 1;
constexpr int kPadding = 6;
constexpr int kSnippetLines = 2;

HelpGuide loadGuide(const QString &languageCode)
{
    // "it_IT" reads the Italian guide; a language without one reads English.
    const QString language = languageCode.section(QLatin1Char('_'), 0, 0).toLower();
    for (const QString &code : {language, QStringLiteral("en")}) {
        QFile file(QStringLiteral(":/help/guide_%1.md").arg(code));
        if (file.open(QIODevice::ReadOnly))
            return HelpGuide::fromMarkdown(QString::fromUtf8(file.readAll()));
    }
    return {};
}

/// A topic of the list: its title and, when searching, the words found in it.
class TopicDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        int height = option.fontMetrics.height() + 2 * kPadding;
        if (!index.data(kSnippetRole).toString().isEmpty())
            height += kSnippetLines * QFontMetrics(snippetFont(option)).lineSpacing() + kPadding / 2;
        return QSize(120, height);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // The background and the selection as the style draws them, the texts here.
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        const QString title = item.text;
        item.text.clear();
        item.widget->style()->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);

        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const QPalette::ColorGroup group = option.state.testFlag(QStyle::State_Enabled) ? QPalette::Normal
                                                                                        : QPalette::Disabled;
        const QColor color = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        const QRect area = option.rect.adjusted(kPadding + 2, kPadding, -kPadding, -kPadding);

        painter->save();
        painter->setPen(color);
        painter->setFont(option.font);
        const QRect titleRect(area.left(), area.top(), area.width(), option.fontMetrics.height());
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          option.fontMetrics.elidedText(title, Qt::ElideRight, titleRect.width()));

        const QString snippet = index.data(kSnippetRole).toString();
        if (!snippet.isEmpty()) {
            QColor aside = color;
            aside.setAlphaF(0.7f);
            painter->setPen(aside);
            painter->setFont(snippetFont(option));
            const QRect snippetRect(area.left(), titleRect.bottom() + kPadding / 2, area.width(),
                                    kSnippetLines * painter->fontMetrics().lineSpacing());
            painter->drawText(snippetRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, snippet);
        }
        painter->restore();
    }

private:
    static QFont snippetFont(const QStyleOptionViewItem &option)
    {
        QFont font = option.font;
        font.setPointSizeF(font.pointSizeF() * 0.88);
        return font;
    }
};

} // namespace

HelpDialog::HelpDialog(const QString &languageCode, QWidget *parent)
    : QDialog(parent)
    , m_guide(loadGuide(languageCode))
    , m_search(new QLineEdit)
    , m_topics(new QListWidget)
    , m_content(new QTextBrowser)
{
    setWindowTitle(tr("Pragma Chess Guide"));
    resize(920, 620);

    m_search->setPlaceholderText(tr("Search the guide"));
    m_search->setClearButtonEnabled(true);
    m_topics->setItemDelegate(new TopicDelegate(m_topics));
    m_topics->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_topics->setAccessibleName(tr("Topics"));
    m_content->setOpenExternalLinks(true);
    m_content->document()->setDocumentMargin(16);

    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_search);
    leftLayout->addWidget(m_topics, 1);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(left);
    splitter->addWidget(m_content);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 640});
    splitter->setChildrenCollapsible(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(splitter);

    connect(m_search, &QLineEdit::textChanged, this, &HelpDialog::listTopics);
    connect(m_topics, &QListWidget::currentRowChanged, this, [this](int row) {
        const QListWidgetItem *item = row >= 0 ? m_topics->item(row) : nullptr;
        if (item && item->data(kTopicRole).isValid())
            showTopic(item->data(kTopicRole).toInt());
    });
    listTopics(QString());
    m_search->setFocus();
}

void HelpDialog::listTopics(const QString &query)
{
    // The topic on show stays selected if it is still listed.
    const QListWidgetItem *current = m_topics->currentItem();
    const int shown = current && current->data(kTopicRole).isValid() ? current->data(kTopicRole).toInt() : -1;

    QList<HelpGuide::Match> matches;
    if (query.trimmed().isEmpty()) {
        for (int index = 0; index < m_guide.topics().size(); ++index)
            matches << HelpGuide::Match{index, QString()};
    } else {
        matches = m_guide.search(query);
    }

    const QSignalBlocker blocker(m_topics);
    m_topics->clear();
    int select = -1;
    for (const HelpGuide::Match &match : std::as_const(matches)) {
        auto *item = new QListWidgetItem(m_guide.topics().at(match.topic).title, m_topics);
        item->setData(kTopicRole, match.topic);
        item->setData(kSnippetRole, match.snippet);
        if (match.topic == shown)
            select = m_topics->count() - 1;
    }
    if (matches.isEmpty()) {
        auto *none = new QListWidgetItem(tr("No topic matches"), m_topics);
        none->setFlags(Qt::NoItemFlags);
        return; // The topic on show stays there.
    }
    // While searching the best match is shown at once; otherwise the topic
    // on show stays, or the guide opens on its first one.
    m_topics->setCurrentRow(query.trimmed().isEmpty() ? qMax(0, select) : 0);
    showTopic(m_topics->currentItem()->data(kTopicRole).toInt());
}

void HelpDialog::showTopic(int index)
{
    if (index < 0 || index >= m_guide.topics().size())
        return;
    m_content->setMarkdown(m_guide.topics().at(index).markdown);

    // The words searched are marked in the text, and the first one is in view.
    QList<QTextEdit::ExtraSelection> marks;
    QTextCharFormat format;
    format.setBackground(palette().color(QPalette::Highlight).lighter(170));
    format.setForeground(palette().color(QPalette::Text));
    for (const QString &word : m_search->text().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        QTextCursor cursor(m_content->document());
        while (!(cursor = m_content->document()->find(word, cursor)).isNull()) {
            QTextEdit::ExtraSelection mark;
            mark.cursor = cursor;
            mark.format = format;
            marks << mark;
        }
    }
    m_content->setExtraSelections(marks);
    if (marks.isEmpty()) {
        m_content->moveCursor(QTextCursor::Start);
    } else {
        QTextCursor first = marks.constFirst().cursor;
        first.clearSelection();
        m_content->setTextCursor(first);
        m_content->ensureCursorVisible();
    }
}
