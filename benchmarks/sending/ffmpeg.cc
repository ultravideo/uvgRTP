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

int main() {
    avcodec_register_all();
    av_register_all();
    avformat_network_init();

    enum AVCodecID codec_id = AV_CODEC_ID_H264;
    AVCodec *codec;
    AVCodecContext *c = NULL;
    int i, ret, x, y, got_output;
    AVFrame *frame;
    AVPacket pkt;

    codec = avcodec_find_encoder(codec_id);
    c = avcodec_alloc_context3(codec);

    c->bit_rate = 400000;
    c->width = 1920;
    c->height = 1080;
    c->time_base.num = 1;
    c->time_base.den = 25;
    c->gop_size = 25;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->flags = AV_CODEC_FLAG_GLOBAL_HEADER;

    if (codec_id == AV_CODEC_ID_H264) {
        ret = av_opt_set(c->priv_data, "preset", "ultrafast", 0);
        ret = av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    }

    avcodec_open2(c, codec, NULL);

    frame = av_frame_alloc();
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;
    ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
        c->pix_fmt, 32);

    AVFormatContext* avfctx;
    AVOutputFormat* fmt = av_guess_format("rtp", NULL, NULL);

#if 0
    ret = avformat_alloc_output_context2(&avfctx, fmt, fmt->name,
        "rtp://127.0.0.1:8888");
#else
    ret = avformat_alloc_output_context2(&avfctx, fmt, fmt->name,
        "rtp://10.21.25.26:8888");
#endif

    avio_open(&avfctx->pb, avfctx->filename, AVIO_FLAG_WRITE);

    struct AVStream* stream = avformat_new_stream(avfctx, codec);
    /* stream->codecpar->bit_rate = 400000; */
    stream->codecpar->width = 3840;
    stream->codecpar->height = 2160;
    stream->codecpar->codec_id = AV_CODEC_ID_HEVC;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->time_base.num = 1;
    stream->time_base.den = 30;

    (void)avformat_write_header(avfctx, NULL);

    uint64_t chunk_size, total_size;
    uint64_t fpt_ms = 0;
    uint64_t fsize  = 0;
    uint32_t frames = 0;

    size_t len = 0;
    void *mem  = get_mem(NULL, NULL, len);

    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t rounds = 0; rounds < 1; ++rounds) {
        for (size_t i = 0; i < len; ) {
            memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

            i          += sizeof(uint64_t);
            total_size += chunk_size;

            av_init_packet(&pkt);
            pkt.data = (uint8_t *)mem + i;
            pkt.size = chunk_size;

            fpt_start = std::chrono::high_resolution_clock::now();
            av_interleaved_write_frame(avfctx, &pkt);
            fpt_end = std::chrono::high_resolution_clock::now();

            uint64_t diff = std::chrono::duration_cast<std::chrono::milliseconds>(fpt_end - fpt_start).count();

            fprintf(stderr, "sleep %d\n", 16 - diff);

            std::this_thread::sleep_for(std::chrono::milliseconds(16 - diff));

            av_packet_unref(&pkt);

            i += chunk_size;
            frames++;
            fsize += chunk_size;
            fpt_ms += diff;
        }
    }
    end = std::chrono::high_resolution_clock::now();

    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %lu ms %lu s\n",
        fsize, fsize / 1000, fsize / 1000 / 1000,
        diff, diff / 1000
    );

    fprintf(stderr, "# of frames: %u\n", frames);
    fprintf(stderr, "avg frame size: %lu\n", fsize / frames);
    fprintf(stderr, "avg processing time of frame: %lu\n", fpt_ms / frames);

    ret = avcodec_send_frame(c, NULL);

    avcodec_close(c);
    av_free(c);
    av_freep(&frame->data[0]);
    av_frame_free(&frame);
    printf("\n");
    return 0;
}
