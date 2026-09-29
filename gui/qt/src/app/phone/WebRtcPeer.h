#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <memory>

namespace rtc {
class DataChannel;
class PeerConnection;
} // namespace rtc

/// One WebRTC peer connection with the single data channel "pragma" of
/// Phone Link (docs/phone-link.md), without trickle ICE: the description is
/// ready once gathering has finished and is exchanged whole.
///
/// libdatachannel calls back on its own threads; every callback is handed to
/// the Qt thread of this object, which is where the signals are emitted.
class WebRtcPeer : public QObject {
    Q_OBJECT

public:
    /// `iceServers` are URLs such as "stun:stun.l.google.com:19302".
    explicit WebRtcPeer(const QStringList &iceServers, QObject *parent = nullptr);
    ~WebRtcPeer() override;

    /// The phone's side: creates the channel; the offer comes with localDescriptionReady.
    void createOffer();
    /// The computer's side: takes the offer; the answer comes with localDescriptionReady.
    bool acceptOffer(const QString &sdp);
    /// The phone's side again: takes the computer's answer.
    bool acceptAnswer(const QString &sdp);

    bool isOpen() const;
    bool sendText(const QByteArray &text);
    bool sendBinary(const QByteArray &data);
    /// Bytes queued on the channel and not yet sent.
    qint64 bufferedAmount() const;
    void close();

    static constexpr qint64 kLowBufferedAmount = 256 * 1024;

Q_SIGNALS:
    /// The whole local description, with every candidate gathered.
    void localDescriptionReady(const QString &sdp);
    void channelOpened();
    void channelClosed();
    void textReceived(const QByteArray &text);
    void binaryReceived(const QByteArray &data);
    /// The buffered amount dropped below kLowBufferedAmount: send more.
    void bufferedAmountLow();
    /// The connection failed (ICE could not connect) or was closed.
    void failed();

private:
    void attachChannel(const std::shared_ptr<rtc::DataChannel> &channel);

    std::shared_ptr<rtc::PeerConnection> m_connection;
    std::shared_ptr<rtc::DataChannel> m_channel;
    bool m_closed = false;
};
