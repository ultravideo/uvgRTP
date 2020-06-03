#include <uvgrtp/lib.hh>
#include <uvgrtp/clock.hh>
#include <cstring>
#include <algorithm>

size_t nframes = 0;

void hook_receiver(void *arg, uvg_rtp::frame::rtp_frame *frame)
{
    auto hevc = (uvg_rtp::media_stream *)arg;
    hevc->push_frame(frame->payload, frame->payload_len, 0);
    nframes++;
}

int receiver(void)
{
    uvg_rtp::context rtp_ctx;
    std::string addr("10.21.25.200");

    auto sess = rtp_ctx.create_session(addr);
    auto hevc = sess->create_stream(
        8888,
        8889,
        RTP_FORMAT_HEVC,
        RCE_SYSTEM_CALL_DISPATCHER
    );

    hevc->install_receive_hook(hevc, hook_receiver);

    while (nframes != 602)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return 0;
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    return receiver();
}
