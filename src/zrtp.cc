#include <cstring>

#include "debug.hh"
#include "random.hh"
#include "zrtp.hh"

#include "mzrtp/commit.hh"
#include "mzrtp/hello.hh"
#include "mzrtp/hello_ack.hh"
#include "mzrtp/receiver.hh"

using namespace kvz_rtp::zrtp_msg;

kvz_rtp::zrtp::zrtp():
    receiver_()
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
    bool hello_recv = false;
    size_t rto      = 50;
    int type        = 0;
    int i           = 0;

    for (i = 0; i < 20; ++i) {
        set_timeout(rto);

        if ((ret = hello.send_msg(socket_, addr_)) != RTP_OK)
            LOG_ERROR("Failed to send Hello message");

        if ((type = receiver_.recv_msg(socket_, 0)) > 0) {
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
                    hello.parse_msg(receiver_, rcapab_);

                    if (rcapab_.version != 110) {
                        LOG_WARN("ZRTP Protocol version %u not supported!", rcapab_.version);
                        hello_recv = false;
                    }
                }

            /* We received ACK for our Hello message.
             * Make sure we've received Hello message also before exiting */
            } else if (type == ZRTP_FT_HELLO_ACK) {
                if (hello_recv)
                    return RTP_OK;
            } else {
                /* at this point we do not care about other messages */
            }
        }

        if (rto < 200)
            rto *= 2;
    }

    /* Hello timed out, perhaps remote did not answer at all or it has an incompatible ZRTP version in use. */
    return RTP_TIMEOUT;
}

rtp_error_t kvz_rtp::zrtp::init_session(bool& initiator)
{
    /* Create ZRTP session from capabilities struct we've constructed
     *
     * TODO cross match the actual capabilities! */
    session_.hash_algo          = S256;
    session_.cipher_algo        = AES1;
    session_.auth_tag_type      = HS32;
    session_.key_agreement_type = DH3k;
    session_.sas_type           = B32;
    session_.hvi[0]             = kvz_rtp::random::generate_32();

    int type        = 0;
    size_t rto      = 0;
    rtp_error_t ret = RTP_OK;
    auto commit     = kvz_rtp::zrtp_msg::commit(session_);

    /* First check if remote has already sent the message.
     * If so, they are the initiator and we're the responder */
    while ((type = receiver_.recv_msg(socket_, MSG_DONTWAIT)) != -EAGAIN) {
        if (type == ZRTP_FT_COMMIT) {
            LOG_ERROR("commit message received early");
            commit.parse_msg(receiver_, session_);
            initiator = false;
            return RTP_OK;
        }
    }

    /* If we proceed to sending Commit message, we can assume we're the initiator.
     * This assumption may prove to be false if remote also sends Commit message
     * and Commit contention is resolved in their favor.
     *
     * */
    initiator = true;
    rto       = 150;

    for (int i = 0; i < 10; ++i) {
        set_timeout(rto);

        if ((ret = commit.send_msg(socket_, addr_)) != RTP_OK)
            LOG_ERROR("Failed to send Commit message!");

        if ((type = receiver_.recv_msg(socket_, 0)) > 0) {

            /* As per RFC 6189, if both parties have sent Commit message and the mode is DH,
             * hvi shall determine who is the initiator (the party with larger hvi is initiator)
             *
             * TODO: do proper check and remove this hack */
            if (type == ZRTP_FT_COMMIT) {
                uint32_t hvi = session_.hvi[0];
                commit.parse_msg(receiver_, session_);

                /* Our hvi is smaller than remote's meaning we are the responder.
                 *
                 * Commit message must be ACKed with DHPart1 messages so we need exit,
                 * construct that message and sent it to remote */
                if (hvi < session_.hvi[0]) {
                    initiator = false;
                    return RTP_OK;
                }
            } else if (type == ZRTP_FT_DH_PART1 || type == ZRTP_FT_CONFIRM1) {
                return RTP_OK;
            }
        }

        if (rto < 1200)
            rto *= 2;
    }

    /* Remote didn't send us any messages, it can be considered dead
     * and ZRTP cannot thus continue any further */
    return RTP_TIMEOUT;
}

rtp_error_t kvz_rtp::zrtp::init(uint32_t ssrc, socket_t& socket, sockaddr_in& addr)
{
    bool initiator  = false;
    rtp_error_t ret = RTP_OK;

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
    if ((ret = begin_session()) != RTP_OK) {
        LOG_ERROR("Session initialization failed, ZRTP cannot be used!");
        return ret;
    }

    /* We're here which means that remote respond to us and sent a Hello message
     * with same version number as ours. This means that the implementations are
     * compatible with each other and we can start the actual negotiation
     *
     * Both participants create Commit messages which include the used algorithms
     * etc. used during the session + some extra information such as ZID
     *
     * init_session() will exchange the Commit messages and select roles for the
     * participants (initiator/responder) based on rules determined in RFC 6189 */
    if ((ret = init_session(initiator)) != RTP_OK) {
        LOG_ERROR("Could not agree on ZRTP session parameters or roles of participants!");
        return ret;
    }

    LOG_INFO("ALL DONE!");

    return RTP_OK;
}
