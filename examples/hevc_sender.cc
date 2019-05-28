#include "../src/lib.hh"
#include "../src/util.hh"
#include "../src/rtp_opus.hh"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <kvazaar.h>
#include <opus/opus.h>
#include <stdint.h>

int main(void)
{
    kvz_rtp::context rtp_ctx;

    kvz_rtp::writer *writer = rtp_ctx.create_writer("127.0.0.1", 8888, RTP_FORMAT_HEVC);
    (void)writer->start();

    uint8_t *buffer = (uint8_t *)malloc(500);
    FILE *inputFile = fopen("video.raw", "r");
    size_t r = 0;

    int width = 1920;
    int height = 1080;

    kvz_encoder* enc = NULL;
    const kvz_api * const api = kvz_api_get(8);
    kvz_config* config = api->config_alloc();
    api->config_init(config);
    api->config_parse(config, "preset", "ultrafast");
    config->width = width;
    config->height = height;
    config->hash = kvz_hash::KVZ_HASH_NONE;
    config->intra_period = 5;
    config->vps_period = 1;
    config->qp = 32;
    config->framerate_num = 30;
    config->framerate_denom = 1;

    enc = api->encoder_open(config);
    if (!enc) {
        fprintf(stderr, "Failed to open encoder.\n");
        return EXIT_FAILURE;
    }

    kvz_picture *img_in[16];
    for (uint32_t i = 0; i < 16; ++i) {
        img_in[i] = api->picture_alloc_csp(KVZ_CSP_420, width, height);
    }

    uint8_t inputCounter = 0;
    uint8_t outputCounter = 0;
    uint32_t frame = 0;
    uint32_t frameIn = 0;
    bool done = false;
    uint8_t *outData = (uint8_t *)malloc(1024 * 1024);

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point end   = std::chrono::high_resolution_clock::now();


    while (!done) {
        kvz_data_chunk* chunks_out = NULL;
        kvz_picture *img_rec = NULL;
        kvz_picture *img_src = NULL;
        uint32_t len_out = 0;
        kvz_frame_info info_out;

        if (!fread(img_in[inputCounter]->y, width*height, 1, inputFile)) {
            done = true;
            continue;
        }
        if (!fread(img_in[inputCounter]->u, width*height>>2, 1, inputFile)) {
            done = true;
            continue;
        }
        if (!fread(img_in[inputCounter]->v, width*height>>2, 1, inputFile)) {
            done = true;
            continue;
        }

        if (!api->encoder_encode(enc,
            img_in[inputCounter],
            &chunks_out, &len_out, &img_rec, &img_src, &info_out))
        {
            fprintf(stderr, "Failed to encode image.\n");
            for (uint32_t i = 0; i < 16; i++) {
                api->picture_free(img_in[i]);
            }
            return EXIT_FAILURE;
        }
        inputCounter = (inputCounter + 1) % 16;


        if (chunks_out == NULL && img_in == NULL) {
            // We are done since there is no more input and output left.
            goto cleanup;
        }

        if (chunks_out != NULL) {
            uint64_t written = 0;
            uint32_t dataPos = 0;

            // Write data into the output file.
            for (kvz_data_chunk *chunk = chunks_out; chunk != NULL; chunk = chunk->next) {
                written += chunk->len;
            }
          
            for (kvz_data_chunk *chunk = chunks_out; chunk != NULL; chunk = chunk->next) {
                memcpy(&outData[dataPos], chunk->data, chunk->len);
                dataPos += chunk->len;
            }

            outputCounter = (outputCounter + 1) % 16;
            frame++;

            if (writer->push_frame(outData, written, RTP_FORMAT_HEVC, (90001 / 24) * frame) < 0) {
                std::cerr << "RTP push failure" << std::endl;
            }
        }
    }

cleanup:
    return 0;
}
