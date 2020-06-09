#include <uvgrtp/lib.hh>
#include <uvgrtp/clock.hh>
#include <cstring>
#include <algorithm>

extern void *get_mem(int argc, char **argv, size_t& len);

std::chrono::high_resolution_clock::time_point start2;

size_t frames   = 0;
size_t ninters  = 0;
size_t nintras  = 0;

size_t total       = 0;
size_t total_intra = 0;
size_t total_inter = 0;

static void hook_sender(void *arg, uvg_rtp::frame::rtp_frame *frame)
{
    (void)arg, (void)frame;

    if (frame) {

        uint64_t diff = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start2
        ).count();

        switch ((frame->payload[0] >> 1) & 0x3f) {
            case 19:
                total += (diff / 1000);
                total_intra += (diff / 1000);
                nintras++, frames++;
                break;

            case 1:
                total += (diff / 1000);
                total_inter += (diff / 1000);
                ninters++, frames++;
                break;
        }
    }
}

static int sender(void)
{
    size_t len          = 0;
    void *mem           = get_mem(0, NULL, len);
    uint64_t csize      = 0;
    uint64_t diff       = 0;
    uint64_t current    = 0;
    uint64_t chunk_size = 0;
    uint64_t period     = (uint64_t)((1000 / (float)30) * 1000);
    rtp_error_t ret     = RTP_OK;
    std::string addr("10.21.25.2");

    uvg_rtp::context rtp_ctx;

    auto sess = rtp_ctx.create_session(addr);
    auto hevc = sess->create_stream(
        8888,
        8889,
        RTP_FORMAT_HEVC,
        RCE_SYSTEM_CALL_DISPATCHER
    );

    hevc->install_receive_hook(nullptr, hook_sender);

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    for (int rounds = 0; rounds < 1; ++rounds) {
        for (size_t offset = 0, k = 0; offset < len; ++k) {
            memcpy(&chunk_size, (uint8_t *)mem + offset, sizeof(uint64_t));

            offset += sizeof(uint64_t);

            start2 = std::chrono::high_resolution_clock::now();
            if ((ret = hevc->push_frame((uint8_t *)mem + offset, chunk_size, 0)) != RTP_OK) {
                fprintf(stderr, "push_frame() failed!\n");
                for (;;);
            }

            auto runtime = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count();

            if (runtime < current * period)
                std::this_thread::sleep_for(std::chrono::microseconds(current * period - runtime));

            current += 1;
            offset  += chunk_size;
        }
    }
    rtp_ctx.destroy_session(sess);

    fprintf(stderr, "intra %lf, inter %lf, avg %lf\n",
        total_intra / (float)nintras,
        total_inter / (float)ninters,
        total / (float)frames
    );

    return 0;
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    return sender();
}
