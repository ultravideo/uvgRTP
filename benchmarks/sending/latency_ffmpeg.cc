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

#include <chrono>
#include <thread>

extern void *get_mem(int argc, char **argv, size_t& len);

#define WIDTH  3840
#define HEIGHT 2160
#define FPS     120
#define SLEEP     8

#define CNT_VAL 1

std::chrono::high_resolution_clock::time_point latency_start, latency_end;

void receiver()
{
    AVFormatContext* format_ctx = avformat_alloc_context();
    AVCodecContext* codec_ctx = NULL;
    int video_stream_index;

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

    if (avformat_open_input(&format_ctx, "../../examples/full/sdp/hevc.sdp", NULL, &d) != 0) {
        return;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        return;
    }

    //search video stream
    for (size_t i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
    }

    size_t cnt = 0;
    size_t size = 0;
    AVPacket packet;
    av_init_packet(&packet);

    //start reading packets from stream and write them to file
    av_read_play(format_ctx);    //play RTSP

    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (++cnt == CNT_VAL) {
            latency_end = std::chrono::high_resolution_clock::now();
            uint64_t diff = std::chrono::duration_cast<std::chrono::microseconds>(latency_end - latency_start).count();

            fprintf(stdout, "%u + ", diff);
            exit(EXIT_SUCCESS);
        }
    }
}

int main() {
    avcodec_register_all();
    av_register_all();
    avformat_network_init();

    enum AVCodecID codec_id = AV_CODEC_ID_H265;
    AVCodec *codec;
    AVCodecContext *c = NULL;
    int i, ret, x, y, got_output;
    AVFrame *frame;
    AVPacket pkt;

    auto recv = new std::thread(receiver);

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

    AVFormatContext* avfctx;
    AVOutputFormat* fmt = av_guess_format("rtp", NULL, NULL);

    /* ret = avformat_alloc_output_context2(&avfctx, fmt, fmt->name, "rtp://10.21.25.2:8888"); */
    ret = avformat_alloc_output_context2(&avfctx, fmt, fmt->name, "rtp://127.0.0.1:8888");

    avio_open(&avfctx->pb, avfctx->filename, AVIO_FLAG_WRITE);

    struct AVStream* stream = avformat_new_stream(avfctx, codec);
    stream->codecpar->width = WIDTH;
    stream->codecpar->height = HEIGHT;
    stream->codecpar->codec_id = AV_CODEC_ID_HEVC;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->time_base.num = 1;
    stream->time_base.den = FPS;

    char buf[256];
    AVDictionary *d = NULL;

    snprintf(buf, sizeof(buf), "%d", 40 * 1024 * 1024);
    av_dict_set(&d, "buffer_size", buf, 32);

    /* Flush the underlying I/O stream after each packet.
     *
     * Default is -1 (auto), which means that the underlying protocol will decide,
     * 1 enables it, and has the effect of reducing the latency,
     * 0 disables it and may increase IO throughput in some cases. */
    snprintf(buf, sizeof(buf), "%d", 1);
    av_dict_set(&d, "flush_packets", NULL, 32);

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
    av_dict_set(&d, "max_interleave_delta", buf, 32);

    /* avioflags flags (input/output)
     *
     * Possible values:
     *   ‘direct’ 
     *      Reduce buffering. */
    snprintf(buf, sizeof(buf), "direct");
    av_dict_set(&d, "avioflags", buf, 32);

    (void)avformat_write_header(avfctx, &d);

    size_t len = 0;
    void *mem  = get_mem(NULL, NULL, len);

    uint64_t chunk_size = 0, diff = 0, counter = 0;

    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t rounds = 0; rounds < 1; ++rounds) {
        for (size_t i = 0; i < len; ) {
            memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

            i += sizeof(uint64_t);

            if (++counter == CNT_VAL) {
                latency_start = std::chrono::high_resolution_clock::now();
            }

            av_init_packet(&pkt);
            pkt.data = (uint8_t *)mem + i;
            pkt.size = chunk_size;

            av_write_frame(avfctx, &pkt);

            i += chunk_size;
        }
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100000));
    }
}
