#pragma once

#include "util.hh"

#include "uvgrtp/export.hh"
#include "uvgrtp/definitions.hh"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2def.h>
#else
#include <netinet/in.h>
#endif

#include <vector>

/* https://stackoverflow.com/questions/1537964/visual-c-equivalent-of-gccs-attribute-packed  */
#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__GNUC__) || defined(__linux__)
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#else
#define PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

namespace uvgrtp {
    namespace frame {

        // enums

        enum RTCP_FRAME_TYPE {
            RTCP_FT_SR    = 200, /* Sender report */
            RTCP_FT_RR    = 201, /* Receiver report */
            RTCP_FT_SDES  = 202, /* Source description */
            RTCP_FT_BYE   = 203, /* Goodbye */
            RTCP_FT_APP   = 204, /* Application-specific message */
            RTCP_FT_RTPFB = 205, /* Transport layer FB message */
            RTCP_FT_PSFB  = 206  /* Payload-specific FB message */
        };

        enum RTCP_RTPFB_FMT {
            RTCP_RTPFB_NACK   = 1 /* Generic NACK, defined in RFC 4585 section 6.2 */
        };

        /*
        enum RTCP_PSFB_FMT {
            RTCP_PSFB_PLI = 1,  // Picture Loss Indication (PLI), defined in RFC 4585
            RTCP_PSFB_SLI = 2,  // Slice Loss Indication (SLI), defined in RFC 4585
            RTCP_PSFB_RPSI = 3,  // Reference Picture Selection Indication (RPSI), defined in RFC 4585
            RTCP_PSFB_FIR = 4,  // Full Intra Request (FIR), defined in RFC 5154
            RTCP_PSFB_TSTR = 5,  // Temporal-Spatial Trade-off Request (TSTR), defined in RFC 5154
            RTCP_PSFB_AFB = 15  // Application Layer FB (AFB), defined in RFC 4585
        };
        */


        // structs

        PACK(struct UVGRTP_EXPORT rtp_header {
            uint8_t version:2;
            uint8_t padding:1;
            uint8_t ext:1;
            uint8_t cc:4;
            uint8_t marker:1;
            uint8_t payload:7;
            uint16_t seq = 0;
            uint32_t timestamp = 0;
            uint32_t ssrc = 0;
        });

        PACK(struct UVGRTP_EXPORT ext_header {
            uint16_t type = 0;
            uint16_t len = 0;
            uint8_t *data = nullptr;
        });

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-5" target="_blank">RFC 3550 section 5</a> */
        struct UVGRTP_EXPORT rtp_frame {
            struct rtp_header header;
            uint32_t *csrc = nullptr;
            struct ext_header *ext = nullptr;

            size_t padding_len = 0; /* non-zero if frame is padded */

            /** \brief Length of the packet payload in bytes added by uvgRTP to help process the frame
            *
            *   \details payload_len = total length - header length - padding length (if padded) 
            */
            size_t payload_len = 0; 
            uint8_t* payload = nullptr;

            /// \cond DO_NOT_DOCUMENT
            uint8_t *dgram = nullptr;      /* pointer to the UDP datagram (for internal use only) */
            size_t   dgram_size = 0;       /* size of the UDP datagram */
            /// \endcond
        };

        /** \brief Header of for all RTCP packets defined in <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6" target="_blank">RFC 3550 section 6</a> */
        struct UVGRTP_EXPORT rtcp_header {
            /** \brief  This field identifies the version of RTP. The version defined by
             * RFC 3550 is two (2).  */
            uint8_t version = 0;
            /** \brief Does this packet contain padding at the end */
            uint8_t padding = 0;
            union {
                /** \brief Source count or report count. Alternative to pkt_subtype. */
                uint8_t count = 0;   
                /** \brief Subtype in APP packets. Alternative to count */
                uint8_t pkt_subtype; 
                /** \brief Feedback message type (FMT), specified in RFC 5104 section 4.3. Alternative to count and pkt_subtype */
                uint8_t fmt;
            };
            /** \brief Identifies the RTCP packet type */
            uint8_t pkt_type = 0;
            /** \brief Length of the whole message measured in 32-bit words */
            uint16_t length = 0;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1" target="_blank">RFC 3550 section 6.4.1</a> */
        struct UVGRTP_EXPORT rtcp_sender_info {
            /** \brief NTP timestamp, most significant word */
            uint32_t ntp_msw = 0;
            /** \brief NTP timestamp, least significant word */
            uint32_t ntp_lsw = 0;
            /** \brief RTP timestamp corresponding to this NTP timestamp */
            uint32_t rtp_ts = 0;
            uint32_t pkt_cnt = 0;
            /** \brief Also known as octet count*/
            uint32_t byte_cnt = 0;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1" target="_blank">RFC 3550 section 6.4.1</a> */
        struct UVGRTP_EXPORT rtcp_report_block {
            uint32_t ssrc = 0;
            uint8_t  fraction = 0;
            int32_t  lost = 0;
            uint32_t last_seq = 0;
            uint32_t jitter = 0;
            uint32_t lsr = 0;  /* last Sender Report */
            uint32_t dlsr = 0; /* delay since last Sender Report */
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1" target="_blank">RFC 3550 section 6.4.1</a> */
        struct UVGRTP_EXPORT rtcp_sr {
            struct rtcp_header header;
            uint32_t ssrc = 0;
            rtcp_sender_info sender_info;
            rtcp_report_block* report_blocks;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.2" target="_blank">RFC 3550 section 6.4.2</a> */
        struct UVGRTP_EXPORT rtcp_rr {
            struct rtcp_header header;
            uint32_t ssrc = 0;
            rtcp_report_block* report_blocks;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
        struct UVGRTP_EXPORT rtcp_sdes_item {
            uint8_t type = 0;
            uint8_t length = 0;
            uint8_t* data = nullptr;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
        struct UVGRTP_EXPORT rtcp_sdes_ck {
            uint32_t ssrc = 0;
            rtcp_sdes_item* items = nullptr;
            size_t item_count = 0;  // not in rfc, here to make usage easier
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
        struct UVGRTP_EXPORT rtcp_sdes {
            struct rtcp_header header;
            rtcp_sdes_ck* chunks = nullptr;
        };

        // \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.7" target="_blank">RFC 3550 section 6.7</a> 
        struct UVGRTP_EXPORT rtcp_app_packet {
            struct rtcp_header header;
            uint32_t ssrc = 0;
            uint8_t name[4] = {0};
            uint8_t *payload = nullptr;
            // \brief Size of the payload in bytes. Added by uvgRTP to help process the payload. 
            size_t payload_len = 0;
        };

        /*
        * The feedback messages are commented as they were not implemented anywhere. If
        * someone wants to implement these, please get rid of the union as it breaks rfc by
        * allowing more than one type of feedback in the same message. 
        * 
        * Please also keep ABI safety in mind by removing std::vector.
        * 

        
        // \brief Full Intra Request, See RFC 5104 section 4.3.1 
        struct rtcp_fir {
            uint32_t ssrc = 0;
            uint8_t seq = 0;
        };
        // \brief Slice Loss Indication, See RFC 4585 section 6.3.2
        struct rtcp_sli {
            uint16_t first = 0;
            uint16_t num = 0;
            uint8_t picture_id = 0;
        };
        // \brief Reference Picture Selection Indication, See RFC 4585 section 6.3.3
        struct rtcp_rpsi {
            uint8_t pb = 0;
            uint8_t pt = 0;
            uint8_t* str = nullptr;
        };

        // \brief RTCP Feedback Control Information, See RFC 4585 section 6.1
        struct rtcp_fb_fci {
            union {
                rtcp_fir fir;
                rtcp_sli sli;
                rtcp_rpsi rpsi;
            };
        };

        // \brief Feedback message. See RFC 4585 section 6.1
        struct rtcp_fb_packet {
            struct rtcp_header header;
            uint32_t sender_ssrc = 0;
            uint32_t media_ssrc = 0;
            std::vector<rtcp_fb_fci> items; // allows illegal stucts, please fix if implementing
            // \brief Size of the payload in bytes. Added by uvgRTP to help process the payload.
            size_t payload_len = 0;
        };

        */

        // dealloc functions, add new functions to the end for abi reasons

        /* Deallocate RTP frame
         *
         * Return RTP_OK on successs
         * Return RTP_INVALID_VALUE if "frame" is nullptr */
        rtp_error_t UVGRTP_EXPORT dealloc_frame(uvgrtp::frame::rtp_frame *frame);

        rtp_error_t UVGRTP_EXPORT dealloc_sr(uvgrtp::frame::rtcp_sr* sr);
        rtp_error_t UVGRTP_EXPORT dealloc_rr(uvgrtp::frame::rtcp_rr* rr);
        rtp_error_t UVGRTP_EXPORT dealloc_sdes(uvgrtp::frame::rtcp_sdes* sdes);

#if UVGRTP_EXTENDED_API
        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.2" target="_blank">RFC 3550 section 6.4.2</a> */
        struct rtcp_receiver_report {
            struct rtcp_header header;
            uint32_t ssrc = 0;
            std::vector<rtcp_report_block> report_blocks;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1" target="_blank">RFC 3550 section 6.4.1</a> */
        struct rtcp_sender_report {
            struct rtcp_header header;
            uint32_t ssrc = 0;
            struct rtcp_sender_info sender_info;
            std::vector<rtcp_report_block> report_blocks;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
        struct rtcp_sdes_chunk {
            uint32_t ssrc = 0;
            std::vector<rtcp_sdes_item> items;
        };

        /** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
        struct rtcp_sdes_packet {
            struct rtcp_header header;
            std::vector<rtcp_sdes_chunk> chunks;
        };
#endif
    }
}

namespace uvg_rtp = uvgrtp;
