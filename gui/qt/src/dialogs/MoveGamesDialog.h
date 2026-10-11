#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

/// The games list's Move…: where the selected games go — a database of the
/// Databases folder, or a new one named here — and whether it is opened when
/// they are there.
class MoveGamesDialog : public QDialog {
    Q_OBJECT

public:
    struct Database {
        QString label; ///< As Switch Database shows it.
        QString path;
    };

    /// `count` games are moved; `databases` are where they may go (not the
    /// open one); `folder` is where a new database is made.
    MoveGamesDialog(int count, const QList<Database> &databases, const QString &folder, bool openWhenDone,
                    QWidget *parent = nullptr);

    /// The database chosen: an existing file, or the path of the new one to create.
    QString path() const;
    bool isNew() const;
    bool openWhenDone() const;

    void accept() override;

private:
    void updateNewName();

    QString m_folder;
    QComboBox *m_target;
    QLabel *m_nameLabel;
    QLineEdit *m_name;
    QLabel *m_problem;
    QCheckBox *m_open;
    QString m_path;
};
