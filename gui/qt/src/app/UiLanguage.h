#pragma once

#include <QList>
#include <QString>

class QCoreApplication;

/// The language of the user interface: the system's, or one the user chose in
/// Options ▸ Language. The choice is a setting of the system user, applied at
/// startup, since widgets are not retranslated while running.
namespace UiLanguage {

struct Language {
    /// "it", "en"; empty for the system language.
    QString code;
    /// The language's own name, e.g. "Italiano".
    QString name;
};

/// The languages the interface can be shown in, the system language first.
QList<Language> available();

/// The language chosen by the user, empty for the system language.
QString chosen();
void setChosen(const QString &code);

/// The language in use: the chosen one, or the system's.
QString effective();

/// Loads the translations of the effective language, for Pragma Chess (from
/// its resources) and for Qt's own dialogs. English needs none.
void install(QCoreApplication &app);

} // namespace UiLanguage
