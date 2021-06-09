#include "zrtp_receiver.hh"

#include "defines.hh"
#include "dh_kxchng.hh"
#include "commit.hh"
#include "confack.hh"
#include "confirm.hh"
#include "hello.hh"
#include "hello_ack.hh"

#include "socket.hh"
#include "crypto.hh"
#include "../poll.hh"
#include "debug.hh"
#include "util.hh"

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#endif

#include <cstring>


using namespace uvgrtp::zrtp_msg;

uvgrtp::zrtp_msg::receiver::receiver()
{
    mem_ = new uint8_t[1024];
    len_  = 1024;
}

uvgrtp::zrtp_msg::receiver::~receiver()
{
    LOG_DEBUG("destroy receiver");
    delete[] mem_;
}

int uvgrtp::zrtp_msg::receiver::recv_msg(uvgrtp::socket *socket, int timeout, int flags)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;
    int nread       = 0;
    rlen_           = 0;

#ifdef _WIN32
    if ((ret = uvgrtp::poll::blocked_recv(socket, mem_, len_, timeout, &nread)) != RTP_OK) {
        if (ret == RTP_INTERRUPTED)
            return -ret;

        log_platform_error("blocked_recv() failed");
        return RTP_RECV_ERROR;
    }
#else
    size_t msec = timeout % 1000;
    size_t sec  = timeout - msec;

    struct timeval tv = {
        (int)sec  / 1000,
        (int)msec * 1000,
    };

    if (socket->setsockopt(SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) != RTP_OK)
        return RTP_GENERIC_ERROR;

    if ((ret = socket->recv(mem_, len_, flags, &nread)) != RTP_OK) {
        if (ret == RTP_INTERRUPTED)
            return -ret;

        log_platform_error("recv(2) failed");
        return RTP_RECV_ERROR;
    }
#endif

    zrtp_msg *msg = (zrtp_msg *)mem_;
    rlen_         = nread;

    if (msg->header.version != 0 || msg->header.magic != ZRTP_HEADER_MAGIC) {
        LOG_DEBUG("Invalid header version or magic");
        return RTP_INVALID_VALUE;
    }

    if (msg->magic != ZRTP_MSG_MAGIC) {
        LOG_DEBUG("invalid ZRTP magic");
        return RTP_INVALID_VALUE;
    }

    switch (msg->msgblock) {
        case ZRTP_MSG_HELLO:
        {
            LOG_DEBUG("Hello message received, verify CRC32!");

            zrtp_hello *hello = (zrtp_hello *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, hello->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_HELLO;

        case ZRTP_MSG_HELLO_ACK:
        {
            LOG_DEBUG("HelloACK message received, verify CRC32!");

            zrtp_hello_ack *ha_msg = (zrtp_hello_ack *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, ha_msg->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_HELLO_ACK;

        case ZRTP_MSG_COMMIT:
        {
            LOG_DEBUG("Commit message received, verify CRC32!");

            zrtp_commit *commit = (zrtp_commit *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, commit->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_COMMIT;

        case ZRTP_MSG_DH_PART1:
        {
            LOG_DEBUG("DH Part1 message received, verify CRC32!");

            zrtp_dh *dh = (zrtp_dh *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_DH_PART1;

        case ZRTP_MSG_DH_PART2:
        {
            LOG_DEBUG("DH Part2 message received, verify CRC32!");

            zrtp_dh *dh = (zrtp_dh *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_DH_PART2;

        case ZRTP_MSG_CONFIRM1:
        {
            LOG_DEBUG("Confirm1 message received, verify CRC32!");

            zrtp_confirm *dh = (zrtp_confirm *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_CONFIRM1;

        case ZRTP_MSG_CONFIRM2:
        {
            LOG_DEBUG("Confirm2 message received, verify CRC32!");

            zrtp_confirm *dh = (zrtp_confirm *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_CONFIRM2;

        case ZRTP_MSG_CONF2_ACK:
        {
            LOG_DEBUG("Conf2 ACK message received, verify CRC32!");

            zrtp_confack *ca = (zrtp_confack *)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, ca->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_CONF2_ACK;

        case ZRTP_MSG_ERROR:
            LOG_DEBUG("Error message received");
            return ZRTP_FT_ERROR;

        case ZRTP_MSG_ERROR_ACK:
            LOG_DEBUG("Error ACK message received");
            return ZRTP_FT_ERROR_ACK;

        case ZRTP_MSG_SAS_RELAY:
            LOG_DEBUG("SAS Relay message received");
            return ZRTP_FT_SAS_RELAY;

        case ZRTP_MSG_RELAY_ACK:
            LOG_DEBUG("Relay ACK message received");
            return ZRTP_FT_RELAY_ACK;

        case ZRTP_MSG_PING_ACK:
            LOG_DEBUG("Ping ACK message received");
            return ZRTP_FT_PING_ACK;
    }

    LOG_WARN("Unknown message type received: 0x%lx", (int)msg->msgblock);
    return RTP_NOT_SUPPORTED;
}

ssize_t uvgrtp::zrtp_msg::receiver::get_msg(void *ptr, size_t len)
{
    if (!ptr || !len)
        return RTP_INVALID_VALUE;

    size_t cpy_len = rlen_;

    if (len < rlen_) {
        cpy_len = len;
        LOG_WARN("Destination buffer too small, cannot copy full message (%zu %zu)!", len, rlen_);
    }

    memcpy(ptr, mem_, cpy_len);
    return rlen_;
}

