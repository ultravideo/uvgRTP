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
#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

using namespace kvz_rtp::zrtp_msg;

enum MSG_TYPES {
    ZRTP_MSG_HELLO     = 0x2020206f6c6c6548,
    ZRTP_MSG_HELLO_ACK = 0x4b43416f6c6c6548,
    ZRTP_MSG_COMMIT    = 0x202074696d6d6f43,
    ZRTP_MSG_DH_PART1  = 0x2031747261504844,
    ZRTP_MSG_DH_PART2  = 0x2032747261504844,
    ZRTP_MSG_CONFIRM1  = 0x316d7269666e6f43,
    ZRTP_MSG_CONFIRM2  = 0x326d7269666e6f43,
    ZRTP_MSG_CONF2_ACK = 0x4b434132666e6f43,
    ZRTP_MSG_ERROR     = 0x202020726f727245,
    ZRTP_MSG_ERROR_ACK = 0x4b4341726f727245,
    ZRTP_MSG_GO_CLEAR  = 0x207261656c436f47,
    ZRTP_MSG_CLEAR_ACK = 0x4b43417261656c43,
    ZRTP_MSG_SAS_RELAY = 0x79616c6572534153,
    ZRTP_MSG_RELAY_ACK = 0x4b434179616c6552,
    ZRTP_MSG_PING      = 0x20202020676e6950,
    ZRTP_MSG_PING_ACK  = 0x204b4341676e6950,
};

kvz_rtp::zrtp_msg::receiver::receiver()
{
    mem_ = new uint8_t[1024];
    len_  = 1024;
}

kvz_rtp::zrtp_msg::receiver::~receiver()
{
    LOG_DEBUG("destroy receiver");
    delete[] mem_;
}

int kvz_rtp::zrtp_msg::receiver::recv_msg(socket_t& socket, int flags)
{
    int nread = 0;
    rlen_     = 0;

    if ((nread = ::recv(socket, mem_, len_, flags)) < 0) {
        if (errno == EAGAIN)
            return -EAGAIN;

        LOG_ERROR("Failed to receive ZRTP Hello message: %d %s!", errno, strerror(errno));
        return -errno;
    }

    /* TODO: validate header */
    zrtp_msg *msg = (zrtp_msg *)mem_;
    rlen_         = nread;

    if (msg->header.version != 0 || msg->header.magic != ZRTP_HEADER_MAGIC) {
        LOG_DEBUG("Invalid header version or magic");
        return -EINVAL;
    }

    if (msg->magic != ZRTP_MSG_MAGIC) {
        LOG_DEBUG("invalid ZRTP magic");
        return -EINVAL;
    }

    switch (msg->msgblock) {
        case ZRTP_MSG_HELLO:
            LOG_DEBUG("Hello message received");
            return ZRTP_FT_HELLO;

        case ZRTP_MSG_HELLO_ACK:
            LOG_DEBUG("HelloACK message received");
            return ZRTP_FT_HELLO_ACK;

        case ZRTP_MSG_COMMIT:
            LOG_DEBUG("Commit message received");
            return ZRTP_FT_COMMIT;

        case ZRTP_MSG_DH_PART1:
            LOG_DEBUG("DH Part1 message received");
            return ZRTP_FT_DH_PART1;

        case ZRTP_MSG_DH_PART2:
            LOG_DEBUG("DH Part2 message received");
            return ZRTP_FT_DH_PART2;

        case ZRTP_MSG_CONFIRM1:
            LOG_DEBUG("Confirm1 message received");
            return ZRTP_FT_CONFIRM1;

        case ZRTP_MSG_CONFIRM2:
            LOG_DEBUG("Confirm2 message received");
            return ZRTP_FT_CONFIRM2;

        case ZRTP_MSG_CONF2_ACK:
            LOG_DEBUG("Conf2 ACK message received");
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
    return -EOPNOTSUPP;
}

ssize_t kvz_rtp::zrtp_msg::receiver::get_msg(void *ptr, size_t len)
{
    if (!ptr || len == 0) {
        return -EINVAL;
    }

    size_t cpy_len = rlen_;

    if (len < rlen_) {
        cpy_len = len;
        LOG_WARN("Destination buffer too small, cannot copy full message (%zu %zu)!", len, rlen_);
    }

    memcpy(ptr, mem_, cpy_len);
    return rlen_;
}
