#include "../src/debug.hh"
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

#define OPUS_BITRATE 64000

// 20 ms for 48 000Hz
#define FRAME_SIZE 960

bool done = false;

void audioSender(RTPContext *ctx, int samplerate, int channels)
{
    /* input has to PCM audio in signed 16-bit little endian format */
    FILE *inFile  = fopen("output.raw", "r");

    int error = 0;
    OpusEncoder* opusEnc = opus_encoder_create(samplerate, channels, OPUS_APPLICATION_VOIP, &error);
    opus_encoder_ctl(opusEnc, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
    opus_encoder_ctl(opusEnc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));

    if (!opusEnc) {
        std::cerr << "opus_encoder_create failed: " << error << std::endl;
        return;
    }

    RTPOpus::OpusConfig *config = new RTPOpus::OpusConfig;
    config->channels            = channels;
    config->samplerate          = samplerate;
    config->configurationNumber = 15; // Hydrib | FB | 20 ms

    RTPWriter *writer = ctx->createWriter("127.0.0.1", 8890, RTP_FORMAT_OPUS);
    writer->start();
    writer->setConfig(config);

    uint32_t dataLenPerFrame = FRAME_SIZE * channels * sizeof(uint16_t);
    uint8_t *inFrame = (uint8_t *)malloc(dataLenPerFrame);
    uint32_t outputSize = 25000;
    uint8_t *outData = (uint8_t *)malloc(outputSize);
    int frame = 0;

    opus_int16 in[FRAME_SIZE * channels];

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point end;

    while (!done) {
        if (!fread(inFrame, dataLenPerFrame, 1, inFile)) {
            done = true;
        } else {
            /* convert from little endian ordering */
            for (int i = 0; i < channels * FRAME_SIZE; ++i) {
                in[i] = inFrame[2 * i + 1] << 8 | inFrame[2 * i];
            }

            int32_t len = opus_encode(opusEnc, in, FRAME_SIZE, outData, outputSize);

            // 20 ms per frame
            if (writer->pushFrame(outData, len, RTP_FORMAT_OPUS, 960 * frame) < 0) { 
                std::cerr << "Failed to push Opus frame!" << std::endl;
            }

            frame++;
            end = std::chrono::high_resolution_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            double diff = 20.0 - (elapsed_time / (double)frame);
            std::this_thread::sleep_for(std::chrono::milliseconds((int)diff));
        }
    }

    opus_encoder_destroy(opusEnc);
    return;
}

int main(void)
{
    RTPContext rtp_ctx;
    std::thread *t = new std::thread(audioSender, &rtp_ctx, 48000, 2);

    t->join();
    std::cerr << "here" << std::endl;
}
