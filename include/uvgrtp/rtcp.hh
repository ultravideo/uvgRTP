#pragma once

#include "clock.hh"
#include "util.hh"
#include "frame.hh"
#include "clock_internal.hh"

#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <bitset>
#include <map>
#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <deque>
#include <atomic>

namespace uvgrtp {

    /// \cond DO_NOT_DOCUMENT
    class socketfactory;
    class rtp;
    class srtcp;
    class rtcp_internal;
    /// \endcond

    /**
     * \brief RTCP instance handles all incoming and outgoing RTCP traffic, including report generation
     *
     * \details If media_stream was created with RCE_RTCP flag, RTCP is enabled. RTCP periodically sends compound RTCP packets. 
     * The bit rate of RTP session influences the reporting interval, but changing this has not yet been implemented.
     *
     * The compound RTCP packet begins with either Sender Reports if we sent RTP packets recently or Receiver Report if we didn't 
     * send RTP packets recently. Both of these report types include report blocks for all the RTP sources we have received packets 
     * from during reporting period. The compound packets also always have an SDES packet and calling send_sdes_packet()-function will 
     * modify the contents of this SDES packet.
     *
     * You can use the APP packet to test new RTCP packet types using the send_app_packet()-function. 
     * The APP packets are added to these periodically sent compound packets.
     * 
     * 
     * See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6" target="_blank">RFC 3550 section 6</a> for more details. 
     */
    class rtcp {
        /// \cond DO_NOT_DOCUMENT
        friend class media_stream_internal;
        /// \endcond
        public:
            /// \cond DO_NOT_DOCUMENT
            ~rtcp();
            /// \endcond

            // the Core api suitable for shared and static library use

            /// \brief Provide timestamping information for RTCP
            ///
            /// \details If the application wishes to timestamp the stream itself and it has
            /// enabled RTCP (via RCE_RTCP), it should provide timestamping information for
            /// RTCP so sensible synchronization values can be calculated for Sender Reports.
            ///
            /// You can use uvgrtp::clock::ntp::now() to get the current NTP timestamp.
            ///
            /// \param clock_start NTP timestamp at t = 0
            /// \param clock_rate Clock rate of the media (Hz)
            /// \param rtp_ts_start RTP timestamp at t = 0
            void set_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start);

            /// \brief Add an SDES item that will be included in all future RTCP SDES packets
            ///
            /// \param item SDES item to include (C-compatible ABI-safe struct)
            /// \retval RTP_OK on success
            /// \retval RTP_GENERIC_ERROR on failure
            rtp_error_t add_sdes_item(const uvgrtp::frame::rtcp_sdes_item& item);

            /// \brief Remove all SDES items previously added with add_sdes_item()
            ///
            /// \details Clears the list of SDES items that are included in outgoing RTCP SDES packets.
            ///
            /// \retval RTP_OK on success
            rtp_error_t clear_sdes_items();

            /// \brief Send an RTCP APP packet
            ///
            /// \param name Four-character name of the APP packet (e.g., "STAT")
            /// \param subtype Subtype (0-31) for the APP message
            /// \param payload_len Size of payload in bytes
            /// \param payload Pointer to payload data
            /// \retval RTP_OK on success
            /// \retval RTP_MEMORY_ERROR if allocation fails
            /// \retval RTP_GENERIC_ERROR if sending fails
            rtp_error_t send_app_packet(const char* name, uint8_t subtype, uint32_t payload_len, const uint8_t* payload);

            /**
             * \brief Install a hook for generating RTCP APP packets during RTCP reporting
             *
             * \details This function installs a C-style callback that is called every time
             * RTCP packets are being prepared for sending. The callback is responsible
             * for returning a pointer to the payload data and filling in subtype and length.
             * The returned buffer must be allocated on the heap using `new uint8_t[]` and will be freed by uvgRTP.
             *
             * \param app_name      Name of the APP item, must be 4 ASCII characters
             * \param send_hook     Function pointer for the hook
             * \param user_arg      User-provided pointer passed to the callback
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If input is invalid
             */
            rtp_error_t install_send_app_hook(
                const char* app_name,
                uint8_t* (*send_hook)(uint8_t* subtype, uint32_t* payload_len, void* user_arg),
                void* user_arg
            );

            /// \brief Remove a previously installed APP sending hook
            /// \param app_name Four-character name of the APP hook to remove
            /// \retval RTP_OK on success
            /// \retval RTP_NOT_FOUND if hook was not found
            rtp_error_t remove_send_app_hook(const char* app_name);

            /// \brief Send an RTCP BYE packet to indicate stream end
            ///
            /// \param ssrcs Array of SSRCs to include in the BYE
            /// \param count Number of SSRCs in the array
            /// \retval RTP_OK on success
            /// \retval RTP_INVALID_VALUE on invalid parameters
            rtp_error_t send_bye_packet(const uint32_t* ssrcs, size_t count);

            /// \brief Install a callback for incoming RTCP Sender Reports
            ///
            /// \param handler Function to call when a Sender Report is received
            /// \retval RTP_OK on success
            /// \retval RTP_INVALID_VALUE if handler is nullptr
            rtp_error_t install_sender_hook(void (*handler)(uvgrtp::frame::rtcp_sr*));

            /// \brief Install a callback for incoming RTCP Receiver Reports
            ///
            /// \param handler Function to call when a Receiver Report is received
            /// \retval RTP_OK on success
            /// \retval RTP_INVALID_VALUE if handler is nullptr
            rtp_error_t install_receiver_hook(void (*handler)(uvgrtp::frame::rtcp_rr*));

            /// \brief Install a callback for incoming RTCP SDES packets
            ///
            /// \param handler Function to call when an SDES packet is received
            /// \retval RTP_OK on success
            /// \retval RTP_INVALID_VALUE if handler is nullptr
            rtp_error_t install_sdes_hook(void (*handler)(uvgrtp::frame::rtcp_sdes*));

            /// \brief Install a callback for incoming RTCP APP packets
            ///
            /// \param handler Function to call when an APP packet is received
            /// \retval RTP_OK on success
            /// \retval RTP_INVALID_VALUE if handler is nullptr
            rtp_error_t install_app_hook(void (*handler)(uvgrtp::frame::rtcp_app_packet*));

            /// \brief Remove all installed reception RTCP packet hook, but not app send hook
            ///
            /// \retval RTP_OK on success
            rtp_error_t remove_all_hooks();

            uvgrtp::frame::rtcp_sr* get_sr(uint32_t ssrc);
            uvgrtp::frame::rtcp_rr* get_rr(uint32_t ssrc);
            uvgrtp::frame::rtcp_sdes* get_sdes(uint32_t ssrc);
            uvgrtp::frame::rtcp_app_packet* get_app_packet(uint32_t ssrc);

#if UVGRTP_EXTENDED_API

            // Extended API

            /**
             * \brief Send an RTCP SDES packet with full SDES items
             *
             * \param items Vector of SDES items
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_sdes_packet(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);

            /**
             * \brief Send an RTCP BYE packet
             *
             * \param ssrcs Vector of SSRCs of those participants who are quitting
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_bye_packet(const std::vector<uint32_t>& ssrcs);

            /**
             * \brief Install a C++ callback for incoming RTCP Sender Reports
             *
             * \param sr_handler C++ function taking std::unique_ptr to Sender Report
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE if handler is nullptr
             */
            rtp_error_t install_sender_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler);

            /**
             * \brief Install a C++ callback for incoming RTCP Receiver Reports
             *
             * \param rr_handler C++ function taking std::unique_ptr to Receiver Report
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE if handler is nullptr
             */
            rtp_error_t install_receiver_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler);

            /**
             * \brief Install a C++ callback for incoming RTCP SDES packets
             *
             * \param sdes_handler C++ function taking std::unique_ptr to SDES packet
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE if handler is nullptr
             */
            rtp_error_t install_sdes_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler);

            /**
             * \brief Install a C++ callback for incoming RTCP APP packets
             *
             * \param app_handler C++ function taking std::unique_ptr to APP packet
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE if handler is nullptr
             */
            rtp_error_t install_app_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler);

            /**
             * \brief Install a C++ hook for sending RTCP APP packets during compound report generation
             *
             * \param app_name Four-character name of the APP item
             * \param app_sending_func Function returning dynamically allocated payload data and setting subtype/payload_len
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE if inputs are invalid
             */
            rtp_error_t install_send_app_hook(const std::string& app_name,
                std::function<std::unique_ptr<uint8_t[]>(uint8_t& subtype, uint32_t& payload_len)> app_sending_func);

            rtp_error_t remove_send_app_hook(const std::string& app_name);


            // deprecated functions

            // replaced by unique_ptr
            [[deprecated("Replaced with unique_ptr or C-style hook functions")]]
            rtp_error_t install_sender_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler);

            [[deprecated("Replaced with unique_ptr or C-style hook functions")]]
            rtp_error_t install_receiver_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler);

            [[deprecated("Replaced with unique_ptr or C-style hook functions")]]
            rtp_error_t install_sdes_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler);

            [[deprecated("Replaced with unique_ptr or C-style hook functions")]]
            rtp_error_t install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler);

            // replaced by ABI-safe versions
            [[deprecated("Use install_sender_hook(rtcp_sr*) from Core API instead")]]
            rtp_error_t install_sender_hook(void (*hook)(uvgrtp::frame::rtcp_sender_report*));

            [[deprecated("Use install_receiver_hook(rtcp_rr*) from Core API instead")]]
            rtp_error_t install_receiver_hook(void (*hook)(uvgrtp::frame::rtcp_receiver_report*));

            [[deprecated("Use install_sdes_hook(rtcp_sdes*) from Core API instead")]]
            rtp_error_t install_sdes_hook(void (*hook)(uvgrtp::frame::rtcp_sdes_packet*));

            [[deprecated("Use get_sr from Core API")]]
            uvgrtp::frame::rtcp_sender_report* get_sender_packet(uint32_t ssrc);

            [[deprecated("Use get_rr from Core API")]]
            uvgrtp::frame::rtcp_receiver_report* get_receiver_packet(uint32_t ssrc);

            [[deprecated("Use get_sdes from Core API")]]
            uvgrtp::frame::rtcp_sdes_packet* get_sdes_packet(uint32_t ssrc);

#endif

        private:

            rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::shared_ptr<std::atomic<std::uint32_t>> ssrc, std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
                std::string cname, std::shared_ptr<uvgrtp::socketfactory> sfp, int rce_flags);
            rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::shared_ptr<std::atomic<std::uint32_t>> ssrc, std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
                std::string cname, std::shared_ptr<uvgrtp::socketfactory> sfp, std::shared_ptr<uvgrtp::srtcp> srtcp, int rce_flags);


            std::shared_ptr<rtcp_internal> pimpl_;
    };
}

namespace uvg_rtp = uvgrtp;
