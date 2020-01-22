#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/hello.hh"
#include "mzrtp/defines.hh"

#define ZRTP_VERSION     "1.10"
#define ZRTP_HELLO       "Hello   "

using namespace kvz_rtp::zrtp_msg;

kvz_rtp::zrtp_msg::hello::hello(zrtp_capab_t& capabilities)
{
    /* We support only the mandatory algorithms etc. defined in RFC 6189
     * so for us all the hash algos and key agreement types are set to zero  */
    size_t common_size = 
        1 * sizeof(uint32_t) + /* magic and length */
        2 * sizeof(uint32_t) + /* message type block */
        1 * sizeof(uint32_t) + /* version */
        4 * sizeof(uint32_t) + /* client indentifer */
        8 * sizeof(uint32_t) + /* hash image */
        3 * sizeof(uint32_t) + /* zid */
        1 * sizeof(uint32_t) + /* header */
        2 * sizeof(uint32_t);  /* mac */

    size_t our_size = common_size;

    /* assume remote supports everything so allocate space for all possible elements */
    size_t their_size = common_size + 5 * 8 * sizeof(uint32_t);

    frame_  = kvz_rtp::frame::alloc_zrtp_frame(our_size);
    rframe_ = kvz_rtp::frame::alloc_zrtp_frame(their_size);

    len_  = sizeof(kvz_rtp::frame::zrtp_frame) + our_size;
    rlen_ = sizeof(kvz_rtp::frame::zrtp_frame) + their_size;

    /* TODO: initialize ZRTP header */

    zrtp_hello *msg = (zrtp_hello *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    /* TODO: convert to network byte order */

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = our_size;

    memcpy(&msg->msg_start.msgblock, ZRTP_HELLO,       8);
    memcpy(&msg->version,            ZRTP_VERSION,     4);
    memcpy(&msg->client,             "kvzRTP",         6);
    memcpy(&msg->zid,                capabilities.zid, 3 * sizeof(uint32_t));

    msg->zero   = 0;
    msg->s      = 0;
    msg->m      = 0;
    msg->p      = 0;
    msg->unused = 0;
    msg->hc     = 0;
    msg->ac     = 0;
    msg->kc     = 0;
    msg->sc     = 0;

    /* TODO: calculate mac */
}

kvz_rtp::zrtp_msg::hello::~hello()
{
    LOG_DEBUG("Freeing ZRTP hello message...");
    (void)kvz_rtp::frame::dealloc_frame(frame_);
}

rtp_error_t kvz_rtp::zrtp_msg::hello::send_msg(socket_t& socket, sockaddr_in& addr)
{
#ifdef __linux
    if (::sendto(socket, (void *)frame_, len_, 0, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        LOG_ERROR("Failed to send ZRTP Hello message: %s!", strerror(errno));
        return RTP_SEND_ERROR;
    }
#else
    /* TODO:  */
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::zrtp_msg::hello::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, zrtp_capab_t& rcapab)
{
    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_hello *msg = (zrtp_hello *)rframe_;

    /* Make sure the version of the message is the one we support.
     * If it's not, there's no reason to parse the message any further */
    if (memcmp(&msg->version, ZRTP_VERSION, 4) == 0)
        rcapab.version = 110;
    else
        return RTP_NOT_SUPPORTED;

    /* TODO: collect preferred algorithms from hello message */

    if (msg->hc != 0) {
    }

    if (msg->cc != 0) {
    }

    if (msg->ac != 0) {
    }

    if (msg->kc != 0) {
    }

    if (msg->sc != 0) {
    }

    /* finally add mandatory algorithms required by the specification to remote capabilities */
    rcapab.hash_algos.push_back(S256);
    rcapab.cipher_algos.push_back(AES1);
    rcapab.auth_tags.push_back(HS32);
    rcapab.auth_tags.push_back(HS80);
    rcapab.key_agreements.push_back(DH3k);
    rcapab.sas_types.push_back(B32);

    /* TODO: verify mac */

    return RTP_OK;
}
