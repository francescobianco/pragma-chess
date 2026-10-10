#pragma once

#include "app/convert/PgnConversion.h"

#include <QDialog>

#include <atomic>
#include <memory>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QThread;

/// Tools ▸ Convert ▸ PGN to Pragma Database…: chooses a PGN file and the
/// database to make of it, and converts on a worker thread (PgnConversion)
/// while the window goes on. Not modal.
class ConvertPgnDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConvertPgnDialog(QWidget *parent = nullptr);
    ~ConvertPgnDialog() override;

    void setPaths(const QString &pgnPath, const QString &pdbPath);
    /// Starts converting the chosen files; false if they cannot be.
    bool start();

    struct Status {
        bool running = false;
        bool finished = false;
        PgnConversion::Result result;
        PgnConversion::Progress progress;
        QString pgnPath;
        QString pdbPath;
    };
    Status status() const { return m_status; }

Q_SIGNALS:
    /// Open Database, after a conversion: the database made.
    void openRequested(const QString &path);

protected:
    void reject() override;

private:
    void choosePgn();
    void choosePdb();
    void showProgress(const PgnConversion::Progress &progress);
    void finished(const PgnConversion::Result &result);
    void updateButtons();

    QLineEdit *m_pgn;
    QLineEdit *m_pdb;
    QPushButton *m_browsePgn;
    QPushButton *m_browsePdb;
    QProgressBar *m_bar;
    QLabel *m_info;
    QPushButton *m_convert;
    QPushButton *m_open;
    QPushButton *m_close;
    QThread *m_thread = nullptr;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_pdbChosen = false; // Chosen by hand: a new PGN file does not change it.
    Status m_status;
};
