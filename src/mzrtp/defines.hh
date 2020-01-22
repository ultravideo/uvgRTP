#pragma once

#include "util.hh"

namespace kvz_rtp {

    namespace zrtp_msg {

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
            Prsh = 0x68737250,
            Mult = 0x746c754d
        };

        enum SAS_TYPES {
            B32  = 0x20323342,
            B256 = 0x36353242
        };
    }
};
