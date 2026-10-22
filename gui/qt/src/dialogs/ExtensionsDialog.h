#pragma once

#include "app/extensions/ExtensionInstaller.h"

#include <QDialog>
#include <QPixmap>

#include <atomic>
#include <functional>
#include <memory>

class ExtensionProvider;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QNetworkAccessManager;
class QProgressBar;
class QThread;
class QPushButton;
class QTreeWidget;

/// Tools ▸ Manage Extensions…: on the left the providers, in the middle what
/// the chosen one offers (searched and filtered by kind), on the right the
/// chosen extension with Install and Remove. Every download is preceded by
/// ExtensionDisclaimerDialog: what is installed is the vendor's, on its
/// terms. Installing puts the files in the extensions' folder
/// (ExtensionInstaller) and configures them: an engine joins Manage Engines
/// through `addEngine`, and leaves it through `removeEngine`; a database is
/// converted into a `.pdb` of the Databases folder (EnCroissantConversion),
/// and what was downloaded goes. Nothing else of the computer is touched.
class ExtensionsDialog : public QDialog {
    Q_OBJECT

public:
    /// `addEngine` configures an installed engine and returns its id in
    /// Manage Engines; `removeEngine` takes it away.
    ExtensionsDialog(std::function<QString(const InstalledExtension &)> addEngine,
                     std::function<void(const QString &engineId)> removeEngine, QWidget *parent = nullptr);
    ~ExtensionsDialog() override;

    /// Shows `provider`'s extensions of `kind` (Engine ▸ Install New Engine…,
    /// Database ▸ Install New Database…); the search is emptied.
    void showOnly(const QString &provider, Extension::Kind kind);
    /// For the development API: chooses the first extension whose name
    /// starts with `name` (once the catalog is read); with `install`, installs
    /// it — the disclaimer asked first, unless `accepted` says it was.
    bool choose(const QString &name, bool install, bool accepted = false);
    /// For the development API: the disclaimer the chosen extension asks.
    QPixmap disclaimerPicture();

protected:
    void reject() override;

private:
    ExtensionProvider *provider() const;
    const Extension *extension() const;
    const InstalledExtension *installedOf(const QString &provider, const QString &id) const;
    void providerChosen();
    void loadLogo(ExtensionProvider *provider, QListWidgetItem *item);
    void fillList();
    void showExtension();
    bool isBusy() const;
    void install(bool accepted = false);
    /// Converts the database just downloaded into the Databases folder.
    void convert(InstalledExtension done);
    void remove();
    void saveInstalled();

    std::function<QString(const InstalledExtension &)> m_addEngine;
    std::function<void(const QString &)> m_removeEngine;
    QList<ExtensionProvider *> m_providers;
    QList<InstalledExtension> m_installed;
    ExtensionInstaller *m_installer;
    /// What is being installed, and its conversion while it runs.
    Extension m_installing;
    QThread *m_conversion = nullptr;
    std::shared_ptr<std::atomic_bool> m_cancelConversion;
    QNetworkAccessManager *m_network;

    QListWidget *m_providerList;
    QLabel *m_providerText;
    QLineEdit *m_search;
    QComboBox *m_kind;
    QTreeWidget *m_list;
    QLabel *m_listStatus;
    QLabel *m_title;
    QLabel *m_facts;
    QLabel *m_note;
    QPushButton *m_install;
    QPushButton *m_remove;
    QProgressBar *m_progress;
    QLabel *m_status;
};
