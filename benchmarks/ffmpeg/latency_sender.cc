extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <stdbool.h>
}

#include <atomic>
#include <deque>
#include <chrono>
#include <string>
#include <thread>
#include <unordered_map>

using namespace std::chrono;

extern void *get_mem(int argc, char **argv, size_t& len);

#define WIDTH  3840
#define HEIGHT 2160
#define FPS      30
#define SLEEP     8

std::chrono::high_resolution_clock::time_point fs, fe;
std::atomic<bool> ready(false);
uint64_t ff_key = 0;

static std::unordered_map<uint64_t, high_resolution_clock::time_point> timestamps;
static std::deque<high_resolution_clock::time_point> timestamps2;

high_resolution_clock::time_point start2;

struct ffmpeg_ctx {
    AVFormatContext *sender;
    AVFormatContext *receiver;
};

static ffmpeg_ctx *init_ffmpeg(const char *ip)
{
    avcodec_register_all();
    av_register_all();
    avformat_network_init();

    av_log_set_level(AV_LOG_PANIC);

    ffmpeg_ctx *ctx = new ffmpeg_ctx;
    enum AVCodecID codec_id = AV_CODEC_ID_H265;
    int i, ret, x, y, got_output;
    AVCodecContext *c = NULL;
    AVCodec *codec;
    AVFrame *frame;
    AVPacket pkt;

    codec = avcodec_find_encoder(codec_id);
    c = avcodec_alloc_context3(codec);

    c->width = HEIGHT;
    c->height = WIDTH;
    c->time_base.num = 1;
    c->time_base.den = FPS;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->flags = AV_CODEC_FLAG_GLOBAL_HEADER;

    avcodec_open2(c, codec, NULL);

    frame = av_frame_alloc();
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;
    ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
        c->pix_fmt, 32);

    AVOutputFormat *fmt = av_guess_format("rtp", NULL, NULL);

    ret = avformat_alloc_output_context2(&ctx->sender, fmt, fmt->name, "rtp://10.21.25.2:8888");

    avio_open(&ctx->sender->pb, ctx->sender->filename, AVIO_FLAG_WRITE);

    struct AVStream* stream = avformat_new_stream(ctx->sender, codec);
    stream->codecpar->width = WIDTH;
    stream->codecpar->height = HEIGHT;
    stream->codecpar->codec_id = AV_CODEC_ID_HEVC;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->time_base.num = 1;
    stream->time_base.den = FPS;

    char buf[256];
    AVDictionary *d_s = NULL;
    AVDictionary *d_r = NULL;

    snprintf(buf, sizeof(buf), "%d", 40 * 1000 * 1000);
    av_dict_set(&d_s, "buffer_size", buf, 32);

#if 1
    /* Flush the underlying I/O stream after each packet.
     *
     * Default is -1 (auto), which means that the underlying protocol will decide,
     * 1 enables it, and has the effect of reducing the latency,
     * 0 disables it and may increase IO throughput in some cases. */
    snprintf(buf, sizeof(buf), "%d", 1);
    av_dict_set(&d_s, "flush_packets", NULL, 32);

    /* Set maximum buffering duration for interleaving. The duration is expressed in microseconds,
     * and defaults to 10000000 (10 seconds).
     *
     * To ensure all the streams are interleaved correctly, libavformat will wait until it has
     * at least one packet for each stream before actually writing any packets to the output file.
     * When some streams are "sparse" (i.e. there are large gaps between successive packets),
     * this can result in excessive buffering.
     *
     * This field specifies the maximum difference between the timestamps of the first and
     * the last packet in the muxing queue, above which libavformat will output a packet regardless of
     * whether it has queued a packet for all the streams.
     *
     * If set to 0, libavformat will continue buffering packets until it has a packet for each stream,
     * regardless of the maximum timestamp difference between the buffered packets. */
    snprintf(buf, sizeof(buf), "%d", 1000);
    av_dict_set(&d_s, "max_interleave_delta", buf, 32);

    /* avioflags flags (input/output)
     *
     * Possible values:
     *   ‘direct’
     *      Reduce buffering. */
    snprintf(buf, sizeof(buf), "direct");
    av_dict_set(&d_s, "avioflags", buf, 32);
#endif

    (void)avformat_write_header(ctx->sender, &d_s);

    /* When sender has been initialized, initialize receiver */
    ctx->receiver = avformat_alloc_context();
    int video_stream_index;

    av_dict_set(&d_r, "protocol_whitelist", "file,udp,rtp", 0);

    /* input buffer size */
    snprintf(buf, sizeof(buf), "%d", 40 * 1000 * 1000);
    av_dict_set(&d_r, "buffer_size", buf, 32);

#if 1
    /* avioflags flags (input/output)
     *
     * Possible values:
     *   ‘direct’
     *      Reduce buffering. */
    snprintf(buf, sizeof(buf), "direct");
    av_dict_set(&d_r, "avioflags", buf, 32);

    /* Reduce the latency introduced by buffering during initial input streams analysis. */
    av_dict_set(&d_r, "nobuffer", NULL, 32);

    /* Set probing size in bytes, i.e. the size of the data to analyze to get stream information.
     *
     * A higher value will enable detecting more information in case it is dispersed into the stream,
     * but will increase latency. Must be an integer not lesser than 32. It is 5000000 by default. */
    snprintf(buf, sizeof(buf), "%d", 32);
    av_dict_set(&d_r, "probesize", buf, 32);

    /*  Set number of frames used to probe fps. */
    snprintf(buf, sizeof(buf), "%d", 2);
    av_dict_set(&d_r, "fpsprobesize", buf, 32);
#endif

    ctx->receiver->flags = AVFMT_FLAG_NONBLOCK;

    if (!strcmp(ip, "127.0.0.1"))
        snprintf(buf, sizeof(buf), "ffmpeg/sdp/localhost/lat_hevc.sdp");
    else
        snprintf(buf, sizeof(buf), "ffmpeg/sdp/lan/lat_hevc.sdp");

    if (avformat_open_input(&ctx->receiver, buf, NULL, &d_r) != 0) {
        fprintf(stderr, "nothing found!\n");
        return NULL;
    }

    for (size_t i = 0; i < ctx->receiver->nb_streams; i++) {
        if (ctx->receiver->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
    }

    return ctx;
}

static void receiver(ffmpeg_ctx *ctx)
{
    uint64_t key  = 0;
    uint64_t diff = 0;

    uint64_t frame_total = 0;
    uint64_t intra_total = 0;
    uint64_t inter_total = 0;

    uint64_t frames = 0;
    uint64_t intras = 0;
    uint64_t inters = 0;

    AVPacket packet;
    av_init_packet(&packet);

    std::chrono::high_resolution_clock::time_point start;
    start = std::chrono::high_resolution_clock::now();

    /* start reading packets from stream */
    av_read_play(ctx->receiver);

    while (av_read_frame(ctx->receiver, &packet) >= 0) {
        key = packet.size - 1;

        if (!frames)
            key = ff_key;

        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - timestamps2.front()
        ).count();
        timestamps2.pop_front();

        if (frames >= 580)
            break;

        if (!(frames % 64))
            intra_total += (diff / 1000), intras++;
        else
            inter_total += (diff / 1000), inters++;

        if (++frames < 596)
            frame_total += (diff / 1000);
        else
            break;

        timestamps.erase(key);

        av_free_packet(&packet);
        av_init_packet(&packet);
    }

    fprintf(stderr, "%zu: intra %lf, inter %lf, avg %lf\n",
        frames,
        intra_total / (float)intras,
        inter_total / (float)inters,
        frame_total / (float)frames
    );

    ready = true;
}

static int sender(void)
{
    size_t len = 0;
    void *mem  = get_mem(0, NULL, len);

    std::string addr("10.21.25.2");
    ffmpeg_ctx *ctx = init_ffmpeg(addr.c_str());

    (void)new std::thread(receiver, ctx);

    uint64_t chunk_size = 0;
    uint64_t diff       = 0;
    uint64_t counter    = 0;
    uint64_t total      = 0;
    uint64_t current    = 0;
    uint64_t key        = 0;
    uint64_t period     = (uint64_t)((1000 / (float)FPS) * 1000);

    AVPacket pkt;
	std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < len; ) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        /* Start code lookup/merging of small packets causes the incoming frame size
         * to differ quite significantly from "chunk_size" */
        if (!i)
            ff_key = chunk_size;

        i += sizeof(uint64_t);

        if (timestamps.find(chunk_size) != timestamps.end()) {
            fprintf(stderr, "cannot use %zu for key!\n", chunk_size);
            continue;
        }

        timestamps[chunk_size] = std::chrono::high_resolution_clock::now();
        timestamps2.push_back(std::chrono::high_resolution_clock::now());

        av_init_packet(&pkt);
        pkt.data = (uint8_t *)mem + i;
        pkt.size = chunk_size;

        av_interleaved_write_frame(ctx->sender, &pkt);
        av_packet_unref(&pkt);

        auto runtime = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        ).count();

        if (runtime < current * period)
            std::this_thread::sleep_for(std::chrono::microseconds(current * period - runtime));

        current++;
        i += chunk_size;
    }

    while (!ready.load())
        ;

    return 0;
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    return sender();
}
