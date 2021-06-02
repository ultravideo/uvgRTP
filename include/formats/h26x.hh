#pragma once

#include "media.hh"
#include "util.hh"

namespace uvgrtp {

    // forward definitions
    class socket;
    class rtp;


    namespace formats {

        class h26x : public media {
            public:
                h26x(uvgrtp::socket *socket, uvgrtp::rtp *rtp, int flags);
                virtual ~h26x();

                /* Find H26x start code from "data"
                 * This process is the same for H26{4,5,6}
                 *
                 * Return the offset of the start code on success
                 * Return -1 if no start code was found */
                ssize_t find_h26x_start_code(uint8_t *data, size_t len, size_t offset, uint8_t& start_len);

                /* Top-level push_frame() called by the Media class
                 * Sets up the frame queue for the send operation
                 *
                 * Return RTP_OK on success
                 * Return RTP_INVALID_VALUE if one of the parameters is invalid */
                rtp_error_t push_media_frame(uint8_t *data, size_t data_len, int flags);

                /* Last push_frame() on the call stack which splits the input frame ("data")
                 * into NAL units using find_h26x_start_code() and fragments the NAL unit
                 * into Fragmentation Units (FUs) which are pushed to frame queue
                 *
                 * Return RTP_OK on success
                 * Return RTP_INVALID_VALUE if one the parameters is invalid */
                rtp_error_t push_h26x_frame(uint8_t *data, size_t data_len, int flags);

            protected:
                /* Each H26x class overrides this function with their custom NAL pushing function */
                virtual rtp_error_t push_nal_unit(uint8_t *data, size_t data_len, bool more);
        };
    };
};

namespace uvg_rtp = uvgrtp;
