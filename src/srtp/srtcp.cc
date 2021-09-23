#include "srtcp.hh"

#include "crypto.hh"
#include "debug.hh"

#include <cstring>
#include <iostream>


uvgrtp::srtcp::srtcp()
{
}

uvgrtp::srtcp::~srtcp()
{
}

rtp_error_t uvgrtp::srtcp::handle_rtcp_encryption(int flags, uint64_t packet_number, 
    uint32_t ssrc, uint8_t* frame, size_t frame_size)
{
    auto ret = RTP_OK;

    /* Encrypt the packet if NULL cipher has not been enabled,
     * calculate authentication tag for the packet and add SRTCP index at the end */
    if (flags & RCE_SRTP) {
        if (!(RCE_SRTP & RCE_SRTP_NULL_CIPHER)) {
            ret = encrypt(ssrc, packet_number, &frame[8], 
                frame_size - 8 - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH);
            SET_FIELD_32(frame, frame_size - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH, 
                (1 << 31) | packet_number);
        }
        else {
            SET_FIELD_32(frame, frame_size - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH, 
                (0 << 31) | packet_number);
        }

        if (ret == RTP_OK)
            ret = add_auth_tag(frame, frame_size);
    }

    return ret;
}

rtp_error_t uvgrtp::srtcp::handle_rtcp_decryption(int flags, uint32_t ssrc, 
    uint8_t* packet, size_t packet_size)
{
    auto ret = RTP_OK;
    auto srtpi = (*(uint32_t*)&packet[packet_size - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH]);

    if (flags & RCE_SRTP) {
        if ((ret = verify_auth_tag(packet, packet_size)) != RTP_OK) {
            LOG_ERROR("Failed to verify RTCP authentication tag!");
            return RTP_AUTH_TAG_MISMATCH;
        }

        if (((srtpi >> 31) & 0x1) && !(flags & RCE_SRTP_NULL_CIPHER)) {
            if (decrypt(ssrc, srtpi & 0x7fffffff, packet, packet_size) != RTP_OK) {
                LOG_ERROR("Failed to decrypt RTCP Sender Report");
                return ret;
            }
        }
    }

    return ret;
}

rtp_error_t uvgrtp::srtcp::encrypt(uint32_t ssrc, uint64_t seq, uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[UVG_IV_LENGTH] = { 0 };

    if (create_iv(iv, ssrc, seq, srtp_ctx_->key_ctx.local.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvgrtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.local.enc_key, srtp_ctx_->n_e, iv);
    ctr.encrypt(buffer, buffer, len);

    return RTP_OK;
}

rtp_error_t uvgrtp::srtcp::add_auth_tag(uint8_t *buffer, size_t len)
{
    auto hmac_sha1 = uvgrtp::crypto::hmac::sha1(srtp_ctx_->key_ctx.local.auth_key, UVG_AES_KEY_LENGTH);

    hmac_sha1.update(buffer, len - UVG_AUTH_TAG_LENGTH);
    hmac_sha1.update((uint8_t *)&srtp_ctx_->roc, sizeof(srtp_ctx_->roc));
    hmac_sha1.final((uint8_t *)&buffer[len - UVG_AUTH_TAG_LENGTH], UVG_AUTH_TAG_LENGTH);

    return RTP_OK;
}

rtp_error_t uvgrtp::srtcp::verify_auth_tag(uint8_t *buffer, size_t len)
{
    uint8_t digest[10] = { 0 };
    auto hmac_sha1     = uvgrtp::crypto::hmac::sha1(srtp_ctx_->key_ctx.remote.auth_key, UVG_AES_KEY_LENGTH);

    hmac_sha1.update(buffer, len - UVG_AUTH_TAG_LENGTH);
    hmac_sha1.update((uint8_t *)&srtp_ctx_->roc, sizeof(srtp_ctx_->roc));
    hmac_sha1.final(digest, UVG_AUTH_TAG_LENGTH);

    if (memcmp(digest, &buffer[len - UVG_AUTH_TAG_LENGTH], UVG_AUTH_TAG_LENGTH)) {
        LOG_ERROR("STCP authentication tag mismatch!");
        return RTP_AUTH_TAG_MISMATCH;
    }

    if (is_replayed_packet(digest)) {
        LOG_ERROR("Replayed packet received, discarding!");
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::srtcp::decrypt(uint32_t ssrc, uint32_t seq, uint8_t *buffer, size_t size)
{
    uint8_t iv[UVG_IV_LENGTH]  = { 0 };

    if (create_iv(iv, ssrc, seq, srtp_ctx_->key_ctx.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvgrtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.remote.enc_key, srtp_ctx_->n_e, iv);

    /* skip header and sender ssrc */
    ctr.decrypt(&buffer[8], &buffer[8], size - 8 - UVG_AUTH_TAG_LENGTH - UVG_SRTCP_INDEX_LENGTH);
    return RTP_OK;
}
