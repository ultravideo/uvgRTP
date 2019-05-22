#pragma once

namespace kvz_rtp {
    class connection;

    namespace sender {

        /* Write RTP Header to OS buffers. This should be called first when sending new RTP packet.
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t write_rtp_header(kvz_rtp::connection *conn, uint32_t timestamp);

        /* To minimize the amount of copying, write all headers directly to operating system's buffers
         * This function uses MSG_MORE/MSG_PARTIAL to prevent the actual sending.
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t write_generic_header(kvz_rtp::connection *conn, uint8_t *header, size_t header_len);

        /* Write the actual data payload and send the full datagram to remote 
         *
         * Caller should first write the RTP and all other necessary headers by calling 
         * write_*_header() and then finalize the send by calling writePayload() 
         *
         * return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t write_payload(kvz_rtp::connection *conn, uint8_t *payload, size_t payload_len);

        /* Write a generic frame 
         * This function sends the datagram immediately,
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t write_generic_frame(kvz_rtp::connection *conn, kvz_rtp::frame::rtp_frame *frame);
    };
};
