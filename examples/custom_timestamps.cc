#include <uvgrtp/lib.hh>

#include <cstdlib>
#include <iostream>
#include <cstring>

/* This example demostrates the usage of custom timestamps. Often when
 * streaming multiple media, there are different lengths steps in in the
 * processing flow. Setting the timestamps manually instead of using uvgRTPs
 * internal timestamp system eliminates the timestamp offset.
 *
 * As a concrete example, audio encoding is usually faster than video encoding and
 * this can cause small offset between video and audio even before reaching uvgRTP.
 * This can be mitigated by capturing the timestamp at the time of recording video/audio
 * and using that as the custom timestamp. This is also how RFC 3550 (RTP) recommends
 * going about this. */

// set these as you want. Take care that the RTCP is set as RTP port +1
constexpr char LOCAL_ADDRESS[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

// RTP clock for video increments 90000 in RTP payload format RFC
constexpr uint32_t VIDEO_CLOCK_RATE = 90000;
constexpr uint32_t VIDEO_FRAME_RATE = 30;

constexpr size_t PAYLOAD_LEN = 100;
constexpr int SEND_TEST_PACKETS = 500;

void process_received_frame_hook(void *arg, uvgrtp::frame::rtp_frame *frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session *local_session, uvgrtp::session *remote_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *receive);

int main(void)
{
    std::cout << "Starting uvgRTP custom timestamp example" << std::endl;

    uvgrtp::context ctx;

    int flags = RCE_RTCP; // enable RTCP
    uvgrtp::session *local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::media_stream *send = local_session->create_stream(LOCAL_PORT, REMOTE_PORT,
                                                              RTP_FORMAT_H265, flags);

    uvgrtp::session *remote_session = ctx.create_session(LOCAL_ADDRESS);
    uvgrtp::media_stream *receive = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT,
                                                                  RTP_FORMAT_H265, flags);

    if (receive)
    {
        /* install receive hook for asynchronous reception */
        if (receive->install_receive_hook(nullptr, process_received_frame_hook) != RTP_OK)
        {
            cleanup(ctx, local_session, remote_session, send, receive);
            return EXIT_FAILURE;
        }
    }

    if (send)
    {
        // RTP specification says there should be a random initial offset
        srand (time(NULL));
        uint32_t start_timestamp  = rand()%UINT32_MAX;

        /* If you don't want uvgRTP to handle timestamping but wish to do that yourself
         * AND you want to use RTCP, timestamping info must be provided for the RTCP so
         * it is able calculate sensible values for synchronization info
         *
         * The first parameter is NTP time associated with the corresponding RTP timestamp,
         * second parameter is clock rate and the third parameter is RTP timestamp for t = 0
         * (it can be zero or some random number, does not matter) */
        send->get_rtcp()->set_ts_info(uvgrtp::clock::ntp::now(), VIDEO_CLOCK_RATE, start_timestamp);

        for (int i = 0; i < SEND_TEST_PACKETS; ++i)
        {
            auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
            memset(buffer.get(), 'a', PAYLOAD_LEN);

            memset(buffer.get(),     0, 3); // start code prefix
            memset(buffer.get() + 3, 1, 1);
            memset(buffer.get() + 4, 1, (19 << 1)); // Intra frame

            // fake timestamp. This way the receiver can play the frames at a lower pace, even if
            // we generate them really fast.
            uint32_t timestamp  = start_timestamp + i*VIDEO_CLOCK_RATE/VIDEO_FRAME_RATE;

            std::cout << "Sending frame " << i + 1 << '/' << SEND_TEST_PACKETS <<
                         " with timestamp: " << timestamp << std::endl;

            /* The timestamp is given as the third parameter and it should be advanced
             * in accordance with the media stream clock rate. For example, for HEVC, the clock rate is 90000. */
            if (send->push_frame(std::move(buffer), PAYLOAD_LEN, timestamp, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send RTP frame!";
                cleanup(ctx, local_session, remote_session, send, receive);
                return EXIT_FAILURE;
            }
        }
    }

    cleanup(ctx, local_session, remote_session, send, receive);
    return EXIT_SUCCESS;
}

void process_received_frame_hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received frame. Timestamp: " << frame->header.timestamp << std::endl;
    uvgrtp::frame::dealloc_frame(frame);
}

void cleanup(uvgrtp::context &ctx, uvgrtp::session *local_session, uvgrtp::session *remote_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *receive)
{
  if (send)
  {
      local_session->destroy_stream(send);
  }

  if (receive)
  {
      remote_session->destroy_stream(receive);
  }

  if (local_session)
  {
      // Session must be destroyed manually
      ctx.destroy_session(local_session);
  }

  if (remote_session)
  {
      // Session must be destroyed manually
      ctx.destroy_session(remote_session);
  }
}

