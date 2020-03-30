#include <stdio.h>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

int main(int argc, char **argv)
{
    AVFormatContext* format_ctx = avformat_alloc_context();
    AVCodecContext* codec_ctx = NULL;
    int video_stream_index;

    /* register everything */
    av_register_all();
    avformat_network_init();

    /* open rtsp */
    AVDictionary *d = NULL;
    av_dict_set(&d, "protocol_whitelist", "file,udp,rtp", 0);

    char buf[256];
    snprintf(buf, sizeof(buf), "%d", 40 * 1024 * 1024);
    av_dict_set(&d, "buffer_size", buf, 0);

    if (avformat_open_input(&format_ctx, "../../examples/full/sdp/hevc.sdp", NULL, &d))
        return EXIT_FAILURE;

    if (avformat_find_stream_info(format_ctx, NULL) < 0)
        return EXIT_FAILURE;

    //search video stream
    for (size_t i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
    }

    size_t cnt = 0;
    size_t size = 0;
    AVPacket packet;
    av_init_packet(&packet);

    /* start reading packets from stream */
    av_read_play(format_ctx);

    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            size += packet.size;
        }

        av_free_packet(&packet);
        av_init_packet(&packet);
    }

    av_read_pause(format_ctx);
    return 0;
}
