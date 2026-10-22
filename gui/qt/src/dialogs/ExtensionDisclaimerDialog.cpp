#include "ExtensionDisclaimerDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

ExtensionDisclaimerDialog::ExtensionDisclaimerDialog(const Extension &extension, const QString &provider,
                                                     bool update, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(update ? tr("Before Updating") : tr("Before Installing"));
    setMinimumWidth(36 * fontMetrics().averageCharWidth() + 12 * fontMetrics().height());
    auto *layout = new QVBoxLayout(this);

    const QString host = QUrl(extension.downloadUrl).host().toHtmlEscaped();
    const QString name = extension.name.toHtmlEscaped();
    QStringList paragraphs;
    paragraphs << QStringLiteral("<b>%1</b>").arg(tr("%1 is not distributed by Pragma Chess").arg(name));
    paragraphs << tr("It is downloaded directly from <a href=\"%1\">%2</a>, where %3 lists it, and installed on "
                     "this computer only. Pragma Chess does not redistribute software or data of others: it "
                     "relies on public and private vendors, who publish what they offer on their own terms.")
                      .arg(extension.downloadUrl.toHtmlEscaped(), host, provider.toHtmlEscaped());
    // Where the terms are read: its author's site when known, else where it comes from.
    const QString termsUrl = extension.homepage.isEmpty() ? extension.downloadUrl : extension.homepage;
    const QString terms = QStringLiteral("<a href=\"%1\">%2</a>")
                              .arg(termsUrl.toHtmlEscaped(), QUrl(termsUrl).host().toHtmlEscaped());
    paragraphs << (extension.license.isEmpty()
                       ? tr("Its vendor states no license here: read its terms on %1 before using it.").arg(terms)
                       : tr("Its terms: %1.").arg(extension.license.toHtmlEscaped()));
    if (extension.kind == Extension::Kind::Database)
        paragraphs << tr("The database is converted into Pragma Chess's format on this computer, for your own use.");
    paragraphs << tr("Respecting those terms is up to you. Pragma Chess does not check what vendors distribute "
                     "and gives no warranty about it: you install it at your own risk.");

    auto *text = new QLabel(QStringLiteral("<p>") + paragraphs.join(QStringLiteral("</p><p>")) + QStringLiteral("</p>"));
    text->setWordWrap(true);
    text->setOpenExternalLinks(true);
    text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    layout->addWidget(text);

    auto *accept = new QCheckBox(tr("I have read this and accept the terms of whoever distributes it"));
    layout->addWidget(accept);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    QPushButton *install = buttons->addButton(update ? tr("Update") : tr("Install"), QDialogButtonBox::AcceptRole);
    install->setEnabled(false);
    connect(accept, &QCheckBox::toggled, install, &QPushButton::setEnabled);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
