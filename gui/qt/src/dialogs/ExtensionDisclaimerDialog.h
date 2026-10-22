#pragma once

#include "app/extensions/Extension.h"

#include <QDialog>

/// Asked before every download of Tools ▸ Manage Extensions, install or
/// update: Pragma Chess does not redistribute software or data of others,
/// it fetches them from the vendors that publish them, whose terms apply.
/// Install stays greyed until the user ticks that they read and accept it.
class ExtensionDisclaimerDialog : public QDialog {
    Q_OBJECT

public:
    /// `provider` is the catalog that lists `extension` ("En Croissant");
    /// `update` names the button Update.
    ExtensionDisclaimerDialog(const Extension &extension, const QString &provider, bool update,
                              QWidget *parent = nullptr);
};
