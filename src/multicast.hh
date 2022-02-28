#pragma once

#include "uvgrtp/util.hh"

namespace uvgrtp {
    class connection;

    namespace frame {
        struct rtp_frame;
    }

    const int MULTICAST_MAX_PEERS = 64;

    class multicast {
        public:
           multicast();
           ~multicast();

           /* Add RTP connection to multicast group */
           rtp_error_t join_multicast(uvgrtp::connection *conn);

           /* TODO:  */
           rtp_error_t leave_multicast(uvgrtp::connection *conn);

           /* TODO:  */
           rtp_error_t push_frame_multicast(uvgrtp::connection *sender, uvgrtp::frame::rtp_frame *frame);

           /* TODO:  */
           rtp_error_t push_frame_multicast(
                uvgrtp::connection *sender,
                uint8_t *data, uint32_t data_len,
                rtp_format_t fmt, uint32_t timestamp
           );
    };
}

namespace uvg_rtp = uvgrtp;
