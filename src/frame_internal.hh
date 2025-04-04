#pragma once

#include "uvgrtp/frame.hh"

namespace uvgrtp
{
    namespace frame
    {
        PACK(struct zrtp_frame {
            uint8_t version : 4;
            uint16_t unused : 12;
            uint16_t seq = 0;
            uint32_t magic = 0;
            uint32_t ssrc = 0;
            uint8_t payload[1];
        });



        /* Allocate an RTP frame
         *
         * First function allocates an empty RTP frame (no payload)
         *
         * Second allocates an RTP frame with payload of size "payload_len",
         *
         * Third allocate an RTP frame with payload of size "payload_len"
         * + probation zone of size "pz_size" * MAX_PAYLOAD
         *
         * Return pointer to frame on success
         * Return nullptr on error and set rtp_errno to:
         *    RTP_MEMORY_ERROR if allocation of memory failed */
        rtp_frame* alloc_rtp_frame();
        rtp_frame* alloc_rtp_frame(size_t payload_len);


        /* Allocate ZRTP frame
         * Parameter "payload_size" defines the length of the frame
         *
         * Return pointer to frame on success
         * Return nullptr on error and set rtp_errno to:
         *    RTP_MEMORY_ERROR if allocation of memory failed
         *    RTP_INVALID_VALUE if "payload_size" is 0 */
        void* alloc_zrtp_frame(size_t payload_size);


        /* Deallocate ZRTP frame
         *
         * Return RTP_OK on successs
         * Return RTP_INVALID_VALUE if "frame" is nullptr */
        rtp_error_t dealloc_frame(uvgrtp::frame::zrtp_frame* frame);
    }
}