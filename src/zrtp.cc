#include <cstring>

#include "debug.hh"
#include "zrtp.hh"

#include "mzrtp/hello.hh"
#include "mzrtp/hello_ack.hh"
#include "mzrtp/receiver.hh"

using namespace kvz_rtp::zrtp_msg;

kvz_rtp::zrtp::zrtp()
{
}

kvz_rtp::zrtp::~zrtp()
{
    delete[] zid_;
}

rtp_error_t kvz_rtp::zrtp::set_timeout(size_t timeout)
{
    struct timeval tv = {
        .tv_sec  = 0,
        .tv_usec = (int)timeout * 1000,
    };

    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        return RTP_GENERIC_ERROR;

    return RTP_OK;
}

kvz_rtp::zrtp_capab_t kvz_rtp::zrtp::get_capabilities()
{
    zrtp_capab_t capabilities = {
    };

    return capabilities;
}

uint8_t *kvz_rtp::zrtp::generate_zid()
{
    return new uint8_t[12];
}

rtp_error_t kvz_rtp::zrtp::begin_session()
{
    rtp_error_t ret = RTP_OK;
    auto hello      = kvz_rtp::zrtp_msg::hello(capab_);
    auto hello_ack  = kvz_rtp::zrtp_msg::hello_ack();
    auto receiver   = kvz_rtp::zrtp_msg::receiver();
    size_t rto      = 50;
    int type        = 0;
    int i           = 0;

    bool hello_recv = false;

    for (i = 0; i < 20; ++i) {
        set_timeout(rto);

        if ((ret = hello.send_msg(socket_, addr_)) != RTP_OK)
            LOG_ERROR("failed to send message");

        if ((type = receiver.recv_msg(socket_)) > 0) {
            /* We received something interesting, either Hello message from remote in which case 
             * we need to send HelloACK message back and keep sending our Hello until HelloACK is received,
             * or HelloACK message which means we can stop sending our  */

            /* We received Hello message from remote, parse it and send  */
            if (type == ZRTP_FT_HELLO) {
                hello_ack.send_msg(socket_, addr_);

                if (!hello_recv) {
                    hello_recv = true;

                    /* Copy interesting information from receiver's
                     * message buffer to remote capabilities struct for later use */
                    hello.parse_msg(receiver, rcapab_);

                    if (rcapab_.version != 110) {
                        LOG_WARN("ZRTP Protocol version %u not supported!", rcapab_.version);
                        hello_recv = false;
                    }
                }
            } else if (type == ZRTP_FT_HELLO_ACK) {
                goto success;
            } else {
                /* at this point we do not care about other messages */
            }
        }

        if (rto < 200)
            rto *= 2;
    }

    /* Hello timed out, perhaps remote did not answer at all or it has an incompatible ZRTP version in use. */
    return RTP_NOT_SUPPORTED;

success:
    /* We have received HelloACK for our Hello message but haven't received Hello from remote,
     * Use rest of the time for waiting it and if it's not heard, session cannot continue */
    if (!hello_recv) {
        rto = (18 - i + 1) * 200 + (i < 2 ? ((i < 1) ? 200 : 150) : 0);
        set_timeout(rto);

        while ((type = receiver.recv_msg(socket_)) != -EAGAIN) {
            if (type == ZRTP_FT_HELLO) {
                hello_ack.send_msg(socket_, addr_);
                hello.parse_msg(receiver, rcapab_);

                if (rcapab_.version != 110)
                    LOG_WARN("ZRTP Protocol version %u not supported!", rcapab_.version);
            }
        }

        return RTP_NOT_SUPPORTED;
    }

    LOG_INFO("Both Hello and HelloACK received");

    return RTP_OK;
}

rtp_error_t kvz_rtp::zrtp::init(uint32_t ssrc, socket_t& socket, sockaddr_in& addr)
{
    ssrc_      = ssrc;
    socket_    = socket;
    addr_      = addr;
    capab_     = get_capabilities();
    capab_.zid = generate_zid();

    /* Begin session by exchanging Hello and HelloACK messages.
     *
     * After begin_session() we know what remote is capable of
     * and whether we are compatible implementations 
     *
     * Remote participant's capabilities are stored to rcapab_ */
    rtp_error_t ret = begin_session();

    if (ret != RTP_OK) {
        LOG_ERROR("Session initialization failed, ZRTP cannot be used!");
        return ret;
    }

    /* TODO: select the initiator */
    /* TODO: initiator sends commit message */
    /* TODO:  */

    LOG_INFO("ALL DONE!");

    return RTP_OK;
}
