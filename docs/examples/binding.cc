#include <uvgrtp/lib.hh>

#include <iostream>

constexpr uint16_t PAYLOAD_MAXLEN = 256;

// The purpose of this example is to show

// change these to reflect your local and remote addresses
// in this example both remote and local are in local host
constexpr char LOCAL_INTERFACE[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8889;

constexpr int SEND_PACKETS = 100;
constexpr int PACKET_INTERVAL_MS = 1000/30;

void hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received frame: Payload size: " << frame->payload_len << std::endl;
    uvgrtp::frame::dealloc_frame(frame);
}

int main(void)
{
    std::cout << "Starting uvgRTP binding example" << std::endl;

    /* See sending.cc for more details */
    uvgrtp::context rtp_ctx;

    /* Start session with remote at IP address LOCAL_INTERFACE
     * and bind ourselves to interface pointed to by the IP address REMOTE_ADDRESS */
    uvgrtp::session *local_session = rtp_ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);

    /* 8888 is source port or the port for the interface where data is received (ie. 10.21.25.200:8888)
     * 8889 is remote port or the port for the interface where the data is sent (ie. 10.21.25.2:8889) */
    uvgrtp::media_stream *send = local_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, RTP_NO_FLAGS);

    /* Some NATs may close the hole created in the firewall if the stream is not bidirectional,
     * i.e., only one participant produces and the other consumes.
     *
     * To prevent the connection from closing, uvgRTP can be instructed to keep the hole open
     * by periodically sending 1-byte datagram to remote (once every 2 seconds).
     *
     * This is done by giving RCE_HOLEPUNCH_KEEPALIVE to the unidirectional media_stream that
     * acts as the receiver
     *
     * All RFC 3550 compatible implementations should ignore the packet as it is not recognized
     * to be a valid RTP frame and the stream should work without problems.
     *
     * NOTE: this flag is only necessary if you're using the created media_stream object
     * as a unidirectional stream and you are noticing that after a while the packets are no longer
     * passing through the firewall */
    uvgrtp::session *remote_session = rtp_ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);
    uvgrtp::media_stream *recv = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT,
                                                               RTP_FORMAT_H265, RCE_HOLEPUNCH_KEEPALIVE);

    if (recv)
    {
        /* install receive hook for asynchronous reception */
        recv->install_receive_hook(nullptr, hook);
    }

    if (send)
    {
        for (unsigned int i = 0; i < SEND_PACKETS; ++i)
        {
            std::unique_ptr<uint8_t[]> buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

            std::cout << "Sending frame " << i << '/' << SEND_PACKETS << std::endl;
            if (send->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send frame" << std::endl;
            }

            // TODO: This is not correct. We should include sending time in this
            std::this_thread::sleep_for(std::chrono::milliseconds(PACKET_INTERVAL_MS));
        }
    }

    if (send)
        local_session->destroy_stream(send);
    if (recv)
        remote_session->destroy_stream(recv);

    if (local_session)
        rtp_ctx.destroy_session(local_session);
    if (remote_session)
        rtp_ctx.destroy_session(remote_session);
}
