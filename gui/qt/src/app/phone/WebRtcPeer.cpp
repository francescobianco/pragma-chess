#include "WebRtcPeer.h"

#include <rtc/rtc.hpp>

namespace {

constexpr char kChannelLabel[] = "pragma";

} // namespace

WebRtcPeer::WebRtcPeer(const QStringList &iceServers, QObject *parent)
    : QObject(parent)
{
    rtc::Configuration config;
    for (const QString &server : iceServers)
        config.iceServers.emplace_back(server.toStdString());
    m_connection = std::make_shared<rtc::PeerConnection>(config);

    m_connection->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
        if (state != rtc::PeerConnection::GatheringState::Complete)
            return;
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (m_closed)
                    return;
                if (const auto description = m_connection->localDescription())
                    Q_EMIT localDescriptionReady(QString::fromStdString(std::string(*description)));
            },
            Qt::QueuedConnection);
    });
    m_connection->onStateChange([this](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Failed || state == rtc::PeerConnection::State::Closed)
            QMetaObject::invokeMethod(
                this, [this] { if (!m_closed) Q_EMIT failed(); }, Qt::QueuedConnection);
    });
    m_connection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> channel) {
        if (channel->label() != kChannelLabel)
            return;
        // Callbacks first, on this thread, so no message is missed.
        attachChannel(channel);
        QMetaObject::invokeMethod(this, [this, channel] { m_channel = channel; }, Qt::QueuedConnection);
    });
}

WebRtcPeer::~WebRtcPeer()
{
    close();
}

void WebRtcPeer::attachChannel(const std::shared_ptr<rtc::DataChannel> &channel)
{
    channel->setBufferedAmountLowThreshold(size_t(kLowBufferedAmount));
    channel->onOpen([this, weak = std::weak_ptr<rtc::DataChannel>(channel)] {
        QMetaObject::invokeMethod(
            this,
            [this, weak] {
                if (m_closed)
                    return;
                if (const auto opened = weak.lock())
                    m_channel = opened;
                Q_EMIT channelOpened();
            },
            Qt::QueuedConnection);
    });
    channel->onClosed([this] {
        QMetaObject::invokeMethod(this, [this] { if (!m_closed) Q_EMIT channelClosed(); }, Qt::QueuedConnection);
    });
    channel->onBufferedAmountLow([this] {
        QMetaObject::invokeMethod(this, [this] { if (!m_closed) Q_EMIT bufferedAmountLow(); }, Qt::QueuedConnection);
    });
    channel->onMessage([this](rtc::message_variant data) {
        if (const auto *text = std::get_if<std::string>(&data)) {
            QByteArray bytes(text->data(), qsizetype(text->size()));
            QMetaObject::invokeMethod(
                this, [this, bytes] { if (!m_closed) Q_EMIT textReceived(bytes); }, Qt::QueuedConnection);
        } else if (const auto *binary = std::get_if<rtc::binary>(&data)) {
            QByteArray bytes(reinterpret_cast<const char *>(binary->data()), qsizetype(binary->size()));
            QMetaObject::invokeMethod(
                this, [this, bytes] { if (!m_closed) Q_EMIT binaryReceived(bytes); }, Qt::QueuedConnection);
        }
    });
}

void WebRtcPeer::createOffer()
{
    // Creating the channel starts the negotiation (and ICE gathering).
    m_channel = m_connection->createDataChannel(kChannelLabel);
    attachChannel(m_channel);
}

bool WebRtcPeer::acceptOffer(const QString &sdp)
{
    try {
        m_connection->setRemoteDescription(rtc::Description(sdp.toStdString(), rtc::Description::Type::Offer));
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

bool WebRtcPeer::acceptAnswer(const QString &sdp)
{
    try {
        m_connection->setRemoteDescription(rtc::Description(sdp.toStdString(), rtc::Description::Type::Answer));
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

bool WebRtcPeer::isOpen() const
{
    return m_channel && m_channel->isOpen();
}

bool WebRtcPeer::sendText(const QByteArray &text)
{
    if (!isOpen())
        return false;
    try {
        m_channel->send(text.toStdString());
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

bool WebRtcPeer::sendBinary(const QByteArray &data)
{
    if (!isOpen())
        return false;
    try {
        m_channel->send(reinterpret_cast<const std::byte *>(data.constData()), size_t(data.size()));
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

qint64 WebRtcPeer::bufferedAmount() const
{
    return m_channel ? qint64(m_channel->bufferedAmount()) : 0;
}

void WebRtcPeer::close()
{
    if (m_closed)
        return;
    m_closed = true;
    // resetCallbacks() waits for a callback in progress: none runs after it.
    if (m_channel) {
        m_channel->resetCallbacks();
        m_channel->close();
    }
    m_connection->resetCallbacks();
    m_connection->close();
}
