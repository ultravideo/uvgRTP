#pragma once

class RTPConnection;

namespace RTPSender {

    /* Write RTP Header to OS buffers. This should be called first when sending new RTP packet.
     *
     * Return RTP_OK on success and RTP_ERROR on error */
    int writeRTPHeader(RTPConnection *conn);

    /* To minimize the amount of copying, write all headers directly to operating system's buffers
     * This function uses MSG_MORE/MSG_PARTIAL to prevent the actual sending.
     *
     * Return RTP_OK on success and RTP_ERROR on error */
    int writeGenericHeader(RTPConnection *conn, uint8_t *header, size_t headerLen);

    /* Write the actual data payload and send the full datagram to remote 
     *
     * Caller should first write the RTP and all other necessary headers by calling 
     * write*Header() and then finalize the sending by calling writePayload() 
     *
     * return RTP_OK on success and RTP_ERROR on error */
    int writePayload(RTPConnection *conn, uint8_t *payload, size_t payloadLen);
};
