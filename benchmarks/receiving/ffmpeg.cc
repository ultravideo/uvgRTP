#include <stdio.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

int main(int argc, char** argv) {

    // Open the initial context variables that are needed
    /* SwsContext *img_convert_ctx; */
    AVFormatContext* format_ctx = avformat_alloc_context();
    AVCodecContext* codec_ctx = NULL;
    int video_stream_index;

    // Register everything
    av_register_all();
    avformat_network_init();

    fprintf(stderr, "\n\n--------------------------------------\nreceiver starting...\n");

    //open RTSP
    AVDictionary *d = NULL;
    av_dict_set(&d, "protocol_whitelist", "file,udp,rtp", 0);
    if (avformat_open_input(&format_ctx, "../../examples/full/sdp/hevc.sdp", NULL, &d) != 0) {
        return EXIT_FAILURE;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        return EXIT_FAILURE;
    }

    //search video stream
    for (size_t i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
    }

    size_t cnt = 0;
    AVPacket packet;
    av_init_packet(&packet);

    //start reading packets from stream and write them to file
    av_read_play(format_ctx);    //play RTSP

    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index)
            fprintf(stderr, "frame %zu read\n", ++cnt);

        av_free_packet(&packet);
        av_init_packet(&packet);
    }

    av_read_pause(format_ctx);

    return (EXIT_SUCCESS);
}
