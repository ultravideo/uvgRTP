#pragma once

#include "util.hh"
#include "frame.hh"

namespace kvz_rtp {
    class connection;

    const int MULTICAST_MAX_PEERS = 64;

    class multicast {
        public:
           multicast();
           ~multicast();

           /* Add RTP connection to multicast group */
           rtp_error_t join_multicast(kvz_rtp::connection *conn);

           /* TODO:  */
           rtp_error_t leave_multicast(kvz_rtp::connection *conn);

           /* TODO:  */
           rtp_error_t push_frame_multicast(kvz_rtp::connection *sender, kvz_rtp::frame::rtp_frame *frame);

           /* TODO:  */
           rtp_error_t push_frame_multicast(
                kvz_rtp::connection *sender,
                uint8_t *data, uint32_t data_len,
                rtp_format_t fmt, uint32_t timestamp
           );
    };
};
