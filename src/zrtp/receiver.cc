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

#include "debug.hh"
#include "util.hh"
#include "crypto.hh"
#include "zrtp/defines.hh"
#include "zrtp/dh_kxchng.hh"
#include "zrtp/commit.hh"
#include "zrtp/confack.hh"
#include "zrtp/confirm.hh"
#include "zrtp/hello.hh"
#include "zrtp/hello_ack.hh"
#include "zrtp/receiver.hh"

using namespace uvg_rtp::zrtp_msg;

uvg_rtp::zrtp_msg::receiver::receiver()
{
    mem_ = new uint8_t[1024];
    len_  = 1024;
}

uvg_rtp::zrtp_msg::receiver::~receiver()
{
    LOG_DEBUG("destroy receiver");
    delete[] mem_;
}

int uvg_rtp::zrtp_msg::receiver::recv_msg(socket_t& socket, int flags)
{
    int nread = 0;
    rlen_     = 0;

    if ((nread = ::recv(socket, mem_, len_, flags)) < 0) {
#ifdef __linux__
        if (errno == EAGAIN || errno == EINTR)
            return -RTP_INTERRUPTED;

        LOG_ERROR("Failed to receive ZRTP Hello message: %d %s!", errno, strerror(errno));
        return -RTP_RECV_ERROR;
#else
#error "platform not supported"
#endif
    }

    /* TODO: validate header */
    zrtp_msg *msg = (zrtp_msg *)mem_;
    rlen_         = nread;

    if (msg->header.version != 0 || msg->header.magic != ZRTP_HEADER_MAGIC) {
        LOG_DEBUG("Invalid header version or magic");
        return -RTP_INVALID_VALUE;
    }

    if (msg->magic != ZRTP_MSG_MAGIC) {
        LOG_DEBUG("invalid ZRTP magic");
        return -RTP_INVALID_VALUE;
    }

    switch (msg->msgblock) {
        case ZRTP_MSG_HELLO:
        {
            LOG_DEBUG("Hello message received, verify CRC32!");

            zrtp_hello *hello = (zrtp_hello *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, hello->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_HELLO;

        case ZRTP_MSG_HELLO_ACK:
        {
            LOG_DEBUG("HelloACK message received, verify CRC32!");

            zrtp_hello_ack *ha_msg = (zrtp_hello_ack *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, ha_msg->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_HELLO_ACK;

        case ZRTP_MSG_COMMIT:
        {
            LOG_DEBUG("Commit message received, verify CRC32!");

            zrtp_commit *commit = (zrtp_commit *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, commit->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_COMMIT;

        case ZRTP_MSG_DH_PART1:
        {
            LOG_DEBUG("DH Part1 message received, verify CRC32!");

            zrtp_dh *dh = (zrtp_dh *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_DH_PART1;

        case ZRTP_MSG_DH_PART2:
        {
            LOG_DEBUG("DH Part2 message received, verify CRC32!");

            zrtp_dh *dh = (zrtp_dh *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_DH_PART2;

        case ZRTP_MSG_CONFIRM1:
        {
            LOG_DEBUG("Confirm1 message received, verify CRC32!");

            zrtp_confirm *dh = (zrtp_confirm *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_CONFIRM1;

        case ZRTP_MSG_CONFIRM2:
        {
            LOG_DEBUG("Confirm2 message received, verify CRC32!");

            zrtp_confirm *dh = (zrtp_confirm *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, dh->crc))
                return RTP_NOT_SUPPORTED;
        }
        return ZRTP_FT_CONFIRM2;

        case ZRTP_MSG_CONF2_ACK:
        {
            LOG_DEBUG("Conf2 ACK message received, verify CRC32!");

            zrtp_confack *ca = (zrtp_confack *)msg;

            if (!uvg_rtp::crypto::crc32::verify_crc32(mem_, rlen_ - 4, ca->crc))
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

    LOG_WARN("Unknown message type received: 0x%lx", msg->msgblock);
    return -RTP_NOT_SUPPORTED;
}

ssize_t uvg_rtp::zrtp_msg::receiver::get_msg(void *ptr, size_t len)
{
    if (!ptr || !len)
        return -RTP_INVALID_VALUE;

    size_t cpy_len = rlen_;

    if (len < rlen_) {
        cpy_len = len;
        LOG_WARN("Destination buffer too small, cannot copy full message (%zu %zu)!", len, rlen_);
    }

    memcpy(ptr, mem_, cpy_len);
    return rlen_;
}

