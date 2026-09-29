#pragma once

#include <QDialog>

class PhoneLink;
class QLabel;
class QLineEdit;
class QTreeWidget;

/// Options ▸ Connect Mobile App…: the QR code the Android app scans to pair
/// with this computer, and the phones already paired, each of which can be
/// disconnected. While it is open the computer accepts one new phone.
class ConnectMobileDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectMobileDialog(PhoneLink *link, QWidget *parent = nullptr);
    ~ConnectMobileDialog() override;

private:
    void updateCode();
    void updateDevices();
    void updateStatus();

    PhoneLink *m_link;
    QLabel *m_code;
    QLineEdit *m_linkText;
    QTreeWidget *m_devices;
    QLabel *m_noDevices;
    QLabel *m_status;
    QString m_lastPaired;
};
