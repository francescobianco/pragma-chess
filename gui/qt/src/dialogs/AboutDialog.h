#pragma once

#include <QDialog>

/// Help ▸ About Pragma Chess, the one About of the application: what it is,
/// the clubs that support it, and what it is built with — Qt, with its logo
/// and its own About a button away, and the other works it ships.
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(const QString &version, QWidget *parent = nullptr);
};
