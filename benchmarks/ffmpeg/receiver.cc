extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

#include <atomic>
#include <cstdio>
#include <chrono>
#include <thread>

struct thread_info {
    size_t pkts;
    size_t bytes;
    std::chrono::high_resolution_clock::time_point start;
} *thread_info;

std::atomic<int> nready(0);

void thread_func(int thread_num)
{
    AVFormatContext *format_ctx = avformat_alloc_context();
    AVCodecContext *codec_ctx = NULL;
    int video_stream_index = 0;

    /* register everything */
    av_register_all();
    avformat_network_init();

    /* open rtsp */
    AVDictionary *d = NULL;
    av_dict_set(&d, "protocol_whitelist", "file,udp,rtp", 0);

    char buf[256];

    /* input buffer size */
    snprintf(buf, sizeof(buf), "%d", 40 * 1024 * 1024);
    av_dict_set(&d, "buffer_size", buf, 32);

    /* avioflags flags (input/output)
     *
     * Possible values:
     *   ‘direct’ 
     *      Reduce buffering. */
    snprintf(buf, sizeof(buf), "direct");
    av_dict_set(&d, "avioflags", buf, 32);

    /* Reduce the latency introduced by buffering during initial input streams analysis. */
    av_dict_set(&d, "nobuffer", NULL, 32);

    /* Set probing size in bytes, i.e. the size of the data to analyze to get stream information.
     *
     * A higher value will enable detecting more information in case it is dispersed into the stream,
     * but will increase latency. Must be an integer not lesser than 32. It is 5000000 by default. */
    snprintf(buf, sizeof(buf), "%d", 256);
    av_dict_set(&d, "probesize", buf, 32);

    /*  Set number of frames used to probe fps. */
    snprintf(buf, sizeof(buf), "%d", 2);
    av_dict_set(&d, "fpsprobesize", buf, 32);

    snprintf(buf, sizeof(buf), "ffmpeg/sdp/hevc_%d.sdp", thread_num / 2);

    if (avformat_open_input(&format_ctx, buf, NULL, &d)) {
        fprintf(stderr, "failed to open input file\n");
        nready++;
        return;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        fprintf(stderr, "failed to find stream info!\n");
        nready++;
        return;
    }

    for (size_t i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
    }

    size_t pkts = 0;
    size_t size = 0;
    AVPacket packet;
    av_init_packet(&packet);

    auto start = std::chrono::high_resolution_clock::now();

    /* start reading packets from stream */
    av_read_play(format_ctx);

    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            size += packet.size;
        } else {
            fprintf(stderr, "unknown format!\n");
        }
        pkts++;

        av_free_packet(&packet);
        av_init_packet(&packet);

        if (++pkts == 597)
            break;
    }

    if (pkts == 597) {
        fprintf(stderr, "%zu %zu\n", size,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );
    } else {
        fprintf(stderr, "discard %zu %zu\n", size,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );
    }

    av_read_pause(format_ctx);
    nready++;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: ./%s <number of threads>\n", __FILE__);
        return -1;
    }

    int nthreads = atoi(argv[1]);
    thread_info  = (struct thread_info *)calloc(nthreads, sizeof(*thread_info));

    for (int i = 0; i < nthreads; ++i)
        new std::thread(thread_func, i * 2);

    while (nready.load() != nthreads)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
}
