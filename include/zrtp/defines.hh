#pragma once

#include "../util.hh"

namespace uvg_rtp {

    namespace zrtp_msg {

        struct zrtp_hello_ack;
        struct zrtp_commit;
        struct zrtp_hello;
        struct zrtp_dh;

        PACKED_STRUCT(zrtp_header) {
            uint8_t version:4;
            uint16_t unused:12;
            uint16_t seq;
            uint32_t magic;
            uint32_t ssrc;
        };

        PACKED_STRUCT(zrtp_msg) {
            struct zrtp_header header;
            uint16_t magic;
            uint16_t length;
            uint64_t msgblock;
        };

        enum ZRTP_FRAME_TYPE {
            ZRTP_FT_HELLO     =  1,
            ZRTP_FT_HELLO_ACK =  2,
            ZRTP_FT_COMMIT    =  3,
            ZRTP_FT_DH_PART1  =  4,
            ZRTP_FT_DH_PART2  =  5,
            ZRTP_FT_CONFIRM1  =  6,
            ZRTP_FT_CONFIRM2  =  7,
            ZRTP_FT_CONF2_ACK =  8,
            ZRTP_FT_SAS_RELAY =  9,
            ZRTP_FT_RELAY_ACK = 10,
            ZRTP_FT_ERROR     = 11,
            ZRTP_FT_ERROR_ACK = 12,
            ZRTP_FT_PING_ACK  = 13
        };

#ifdef _MSC_VER
        enum ZRTP_MSG_TYPE: __int64 {
#else
        enum ZRTP_MSG_TYPE {
#endif
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

        enum MAGIC {
            ZRTP_HEADER_MAGIC = 0x5a525450,
            ZRTP_MSG_MAGIC    = 0x0000505a,
        };

        enum HASHES {
            S256 = 0x36353253,
            S384 = 0x34383353,
            N256 = 0x3635324e,
            N384 = 0x3438334e
        };

        enum CIPHERS {
            AES1 = 0x31534541,
            AES2 = 0x32534541,
            AES3 = 0x33534541,
            TFS1 = 0x31534632,
            TFS2 = 0x32534632,
            TFS3 = 0x33534632
        };

        enum AUTH_TAGS {
            HS32 = 0x32335348,
            HS80 = 0x30385348,
            SK32 = 0x32334b53,
            SK64 = 0x34364b53
        };

        enum KEY_AGREEMENT {
            DH3k = 0x6b334844,
            DH2k = 0x6b324844,
            EC25 = 0x35324345,
            EC38 = 0x38334345,
            EC52 = 0x32354345,
            PRSH = 0x68737250,
            MULT = 0x746c754d
        };

        enum SAS_TYPES {
            B32  = 0x20323342,
            B256 = 0x36353242
        };

        enum ERRORS {
            ZRTP_ERR_MALFORED_PKT        = 0x10,  /* Malformed packet */
            ZRTP_ERR_SOFTWARE            = 0x20,  /* Critical software error */
            ZRTP_ERR_VERSION             = 0x30,  /* Unsupported version */
            ZRTP_ERR_COMPONENT_MISMATCH  = 0x40,  /* Hello message component mismatch */
            ZRTP_ERR_NS_HASH_TYPE        = 0x51,  /* Hash type not supported */
            ZRTP_ERR_NS_CIPHER_TYPE      = 0x52,  /* Cipher type not supported  */
            ZRTP_ERR_NS_PBKEY_EXCHANGE   = 0x53,  /* Public key exchange not supported */
            ZRTP_ERR_NS_SRTP_AUTH_TAG    = 0x54,  /* SRTP auth tag not supported */
            ZRTP_ERR_NS_SAS_RENDERING    = 0x55,  /* SAS Rendering Scheme not supported */
            ZRTP_ERR_NO_SHARED_SECRET    = 0x56,  /* No shared secret available */
            ZRTP_ERR_DHE_BAD_PVI         = 0x61,  /* DH Error: Bad pvi or pvr */
            ZRTP_ERR_DHE_HVI_MISMATCH    = 0x62,  /* DH Error: hvi != hashed data */
            ZRTP_ERR_UNTRUSTED_MITM      = 0x63,  /* Received relayed SAS from untrusted MiTM */
            ZRTP_ERR_BAD_CONFIRM_MAC     = 0x70,  /* Bad Confirm Packet MAC */
            ZRTP_ERR_NONCE_REUSE         = 0x80,  /* Nonce reuse */
            ZRTP_ERR_EQUAL_ZID           = 0x90,  /* Equal ZID in Hello */
            ZRTP_ERR_SSRC_COLLISION      = 0x91,  /* SSRC collision */
            ZRTP_ERR_SERVICE_UNAVAILABLE = 0xA0,  /* Service unavailable */
            ZRTP_ERR_PROTOCOL_TIMEOUT    = 0xB0,  /* Protocol timeout error */
            ZRTP_ERR_GOCLEAR_NOT_ALLOWED = 0x100, /* Goclear received but not supported */
        };
    };
};
