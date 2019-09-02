#include <iostream>
#include <cstdio>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <kvazaar.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

int kvazaar_encode(char *input, char *output);

void *get_mem(int argc, char **argv, size_t& len)
{
    char *input  = NULL;
    char *output = NULL;

    if (argc != 3) {
        input  = (char *)"util/video.raw";
        output = (char *)"util/out.hevc";
    } else {
        input  = argv[1];
        output = argv[2];
    }

    if (access(output, F_OK) == -1) {
        (void)kvazaar_encode(input, output);
    }

    int fd = open(output, O_RDONLY, 0);

    struct stat st;
    stat(output, &st);
    len = st.st_size;

    return mmap(NULL, len, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
}

int get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len)
{
    uint8_t zeros = 0;
    uint32_t pos  = 0;

    while (offset + pos < data_len) {
        if (zeros >= 2 && data[offset + pos] == 1) {
            start_len = zeros + 1;
            return offset + pos + 1;
        }

        if (data[offset + pos] == 0)
            zeros++;
        else
            zeros = 0;

        pos++;
    }

    return -1;
}

int kvazaar_encode(char *input, char *output)
{
    FILE *inputFile  = fopen(input, "r");
    FILE *outputFile = fopen(output, "w");

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
    config->qp = 25;
    config->framerate_num = 120;
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
    bool done = false;
    int r = 0;

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

            // Write data into the output file.
            for (kvz_data_chunk *chunk = chunks_out; chunk != NULL; chunk = chunk->next) {
                written += chunk->len;
            }
          
            fprintf(stderr, "write chunk size: %lu\n", written);
            fwrite(&written, sizeof(uint64_t), 1, outputFile);
            for (kvz_data_chunk *chunk = chunks_out; chunk != NULL; chunk = chunk->next) {
                fwrite(chunk->data, chunk->len, 1, outputFile);
            }

            outputCounter = (outputCounter + 1) % 16;

            /* if (++r > 5) */
            /*     goto cleanup; */
        }
    }

cleanup:
    fclose(inputFile);
    fclose(outputFile);

    return 0;
}
