#pragma once

#include <vector>
#include <memory>

#include "frame.hh"
#include "util.hh"

namespace uvg_rtp {
    class sender;
    class receiver;

    namespace generic {

        /* TODO:  */
        rtp_error_t push_frame(uvg_rtp::sender *sender, uint8_t *data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t push_frame(uvg_rtp::sender *sender, std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t frame_receiver(uvg_rtp::receiver *receiver);
    };
};
