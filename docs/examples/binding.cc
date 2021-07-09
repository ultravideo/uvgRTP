#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 256

void hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    uvgrtp::frame::dealloc_frame(frame);
}

int main(void)
{
    /* See sending.cc for more details */
    uvgrtp::context rtp_ctx;

    /* Start session with remote at IP address 10.21.25.2
     * and bind ourselves to interface pointed to by the IP address 10.21.25.200 */
    uvgrtp::session *s1 = rtp_ctx.create_session("10.21.25.2", "10.21.25.200");

    /* 8888 is source port or the port for the interface where data is received (ie. 10.21.25.200:8888)
     * 8889 is remote port or the port for the interface where the data is sent (ie. 10.21.25.2:8889) */
    uvgrtp::media_stream *send = s1->create_stream(8888, 8889, RTP_FORMAT_H265, RTP_NO_FLAGS);

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
    uvgrtp::media_stream *recv = s1->create_stream(7777, 6666, RTP_FORMAT_H265, RCE_HOLEPUNCH_KEEPALIVE);

    /* install receive hook for asynchronous reception */
    recv->install_receive_hook(nullptr, hook);

    while (true) {
        std::unique_ptr<uint8_t[]> buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        if (send->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK)
            fprintf(stderr, "failed to push hevc frame\n");

        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    rtp_ctx.destroy_session(s1);
}
