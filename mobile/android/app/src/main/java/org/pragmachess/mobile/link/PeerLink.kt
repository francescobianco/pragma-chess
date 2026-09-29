package org.pragmachess.mobile.link

import android.content.Context
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull
import org.webrtc.DataChannel
import org.webrtc.IceCandidate
import org.webrtc.MediaConstraints
import org.webrtc.MediaStream
import org.webrtc.PeerConnection
import org.webrtc.PeerConnectionFactory
import org.webrtc.RtpReceiver
import org.webrtc.SdpObserver
import org.webrtc.SessionDescription
import java.nio.ByteBuffer
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/** A frame received on the data channel. */
sealed interface Frame {
    data class Text(val text: String) : Frame
    class Binary(val data: ByteArray) : Frame
    data object Closed : Frame
}

/**
 * The phone's end of the WebRTC connection: it offers, with the one data
 * channel "pragma", and sends its SDP whole once ICE gathering is over.
 */
class PeerLink(context: Context) : AutoCloseable {
    val frames = Channel<Frame>(Channel.UNLIMITED)
    private val gathered = CompletableDeferred<Unit>()
    private val open = CompletableDeferred<Boolean>()
    private val connection: PeerConnection
    private val channel: DataChannel

    init {
        val config = PeerConnection.RTCConfiguration(ICE_SERVERS.map { PeerConnection.IceServer.builder(it).createIceServer() }).apply {
            sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN
            continualGatheringPolicy = PeerConnection.ContinualGatheringPolicy.GATHER_ONCE
        }
        connection = factory(context).createPeerConnection(config, object : PeerConnection.Observer {
            override fun onIceGatheringChange(state: PeerConnection.IceGatheringState) {
                if (state == PeerConnection.IceGatheringState.COMPLETE) gathered.complete(Unit)
            }

            override fun onIceConnectionChange(state: PeerConnection.IceConnectionState) {
                if (state == PeerConnection.IceConnectionState.FAILED) open.complete(false)
            }

            override fun onSignalingChange(state: PeerConnection.SignalingState) = Unit
            override fun onIceConnectionReceivingChange(receiving: Boolean) = Unit
            override fun onIceCandidate(candidate: IceCandidate) = Unit
            override fun onIceCandidatesRemoved(candidates: Array<out IceCandidate>) = Unit
            override fun onAddStream(stream: MediaStream) = Unit
            override fun onRemoveStream(stream: MediaStream) = Unit
            override fun onDataChannel(channel: DataChannel) = Unit
            override fun onRenegotiationNeeded() = Unit
            override fun onAddTrack(receiver: RtpReceiver, streams: Array<out MediaStream>) = Unit
        }) ?: error("WebRTC is not available")
        channel = connection.createDataChannel("pragma", DataChannel.Init().apply { ordered = true })
        channel.registerObserver(object : DataChannel.Observer {
            override fun onBufferedAmountChange(previousAmount: Long) = Unit

            override fun onStateChange() {
                when (channel.state()) {
                    DataChannel.State.OPEN -> open.complete(true)
                    DataChannel.State.CLOSED -> {
                        open.complete(false)
                        frames.trySend(Frame.Closed)
                    }
                    else -> Unit
                }
            }

            override fun onMessage(buffer: DataChannel.Buffer) {
                val bytes = ByteArray(buffer.data.remaining())
                buffer.data.get(bytes)
                frames.trySend(if (buffer.binary) Frame.Binary(bytes) else Frame.Text(String(bytes, Charsets.UTF_8)))
            }
        })
    }

    /** The offer with every ICE candidate gathered within [gatherMs]. */
    suspend fun createOffer(gatherMs: Long = 8000): String {
        val offer = suspendCancellableCoroutine { cont ->
            connection.createOffer(object : SdpObserverAdapter() {
                override fun onCreateSuccess(description: SessionDescription) = cont.resume(description)
                override fun onCreateFailure(error: String?) = cont.resumeWithException(IllegalStateException(error))
            }, MediaConstraints())
        }
        setDescription(offer, local = true)
        withTimeoutOrNull(gatherMs) { gathered.await() }
        return connection.localDescription?.description ?: offer.description
    }

    suspend fun acceptAnswer(sdp: String) =
        setDescription(SessionDescription(SessionDescription.Type.ANSWER, sdp), local = false)

    private suspend fun setDescription(description: SessionDescription, local: Boolean) =
        suspendCancellableCoroutine { cont ->
            val observer = object : SdpObserverAdapter() {
                override fun onSetSuccess() = cont.resume(Unit)
                override fun onSetFailure(error: String?) = cont.resumeWithException(IllegalStateException(error))
            }
            if (local) connection.setLocalDescription(observer, description)
            else connection.setRemoteDescription(observer, description)
        }

    /** True once the data channel is open, false if ICE failed or [timeoutMs] passed. */
    suspend fun awaitOpen(timeoutMs: Long): Boolean = withTimeoutOrNull(timeoutMs) { open.await() } ?: false

    fun send(text: String): Boolean = channel.send(DataChannel.Buffer(ByteBuffer.wrap(text.toByteArray(Charsets.UTF_8)), false))

    override fun close() {
        try {
            channel.unregisterObserver()
            channel.close()
            channel.dispose()
            connection.dispose()
        } catch (e: Exception) {
            // Already gone.
        }
        frames.trySend(Frame.Closed)
    }

    private abstract class SdpObserverAdapter : SdpObserver {
        override fun onCreateSuccess(description: SessionDescription) = Unit
        override fun onSetSuccess() = Unit
        override fun onCreateFailure(error: String?) = Unit
        override fun onSetFailure(error: String?) = Unit
    }

    companion object {
        val ICE_SERVERS = listOf("stun:stun.l.google.com:19302", "stun:stun.cloudflare.com:3478")

        @Volatile private var sharedFactory: PeerConnectionFactory? = null

        private fun factory(context: Context): PeerConnectionFactory = sharedFactory ?: synchronized(this) {
            sharedFactory ?: run {
                PeerConnectionFactory.initialize(
                    PeerConnectionFactory.InitializationOptions.builder(context.applicationContext).createInitializationOptions()
                )
                PeerConnectionFactory.builder().createPeerConnectionFactory().also { sharedFactory = it }
            }
        }
    }
}
