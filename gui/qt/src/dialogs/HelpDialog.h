#pragma once

#include "app/HelpGuide.h"

#include <QDialog>

class QLineEdit;
class QListWidget;
class QTextBrowser;

/// The guide of Pragma Chess: the topics on the left, under a search field,
/// and the chosen topic on the right. Searching leaves only the topics that
/// match, each with the words around what was found. The guide is the one
/// of the interface language (HelpGuide, `:/help/guide_<code>.md`), English
/// when there is none.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(const QString &languageCode, QWidget *parent = nullptr);

private:
    /// Lists the topics matching `query`, or all of them when it is empty.
    void listTopics(const QString &query);
    void showTopic(int index);

    HelpGuide m_guide;
    QLineEdit *m_search;
    QListWidget *m_topics;
    QTextBrowser *m_content;
};
