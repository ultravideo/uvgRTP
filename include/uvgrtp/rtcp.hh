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

            ~rtcp();

            /* Send "frame" to all participants
             *
             * These routines will convert all necessary fields to network byte order
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "frame" is in some way invalid
             * Return RTP_SEND_ERROR if sending "frame" did not succeed (see socket.hh for details) */

            /**
             * \brief Send an RTCP SDES packet
             *
             * \param items Vector of SDES items
             *
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_sdes_packet(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);

            /**
             * \brief Send an RTCP APP packet
             *
             * \param name Name of the APP item, e.g., STAT, must have a length of four ASCII characters
             * \param subtype Subtype of the APP item
             * \param payload_len Length of the payload
             * \param payload Payload
             *
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_app_packet(const char *name, uint8_t subtype, uint32_t payload_len, const uint8_t *payload);

            /**
             * \brief Send an RTCP BYE packet
             *
             * \details In case the quitting participant is a mixer and is serving multiple
             * paricipants, the input vector contains the SSRCs of all those participants. If the
             * participant is a regular member of the session, the vector only contains the SSRC
             * of the participant.
             *
             * \param ssrcs Vector of SSRCs of those participants who are quitting
             *
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_bye_packet(std::vector<uint32_t> ssrcs);

            /**
             * \brief Provide timestamping information for RTCP
             *
             * \details If the application wishes to timestamp the stream itself AND it has
             * enabled RTCP by using ::RCE_RTCP, it must provide timestamping information for
             * RTCP so sensible synchronization values can be calculated for Sender Reports
             *
             * The application can call uvgrtp::clock::ntp::now() to get the current wall clock
             * reading as an NTP timestamp value
             *
             * \param clock_start NTP timestamp for t = 0
             * \param clock_rate Clock rate of the stream
             * \param rtp_ts_start RTP timestamp for t = 0
             */
            void set_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start);

            /* Alternate way to get RTCP packets is to install a hook for them. So instead of
             * polling an RTCP packet, user can install a function that is called when
             * a specific RTCP packet is received. */

            /**
             * \brief Install an RTCP Sender Report hook
             *
             * \details This function is called when an RTCP Sender Report is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sender_hook(void (*hook)(uvgrtp::frame::rtcp_sender_report *));

            /**
             * \brief Install an RTCP Sender Report hook
             *
             * \details This function is called when an RTCP Sender Report is received
             *
             * \param sr_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sender_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler);

            /**
             * \brief Install an RTCP Receiver Report hook
             *
             * \details This function is called when an RTCP Receiver Report is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_receiver_hook(void (*hook)(uvgrtp::frame::rtcp_receiver_report *));

            /**
             * \brief Install an RTCP Receiver Report hook
             *
             * \details This function is called when an RTCP Receiver Report is received
             *
             * \param rr_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_receiver_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler);

            /**
             * \brief Install an RTCP SDES packet hook
             *
             * \details This function is called when an RTCP SDES packet is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sdes_hook(void (*hook)(uvgrtp::frame::rtcp_sdes_packet *));

            /**
             * \brief Install an RTCP SDES packet hook
             *
             * \details This function is called when an RTCP SDES packet is received
             *
             * \param sdes_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sdes_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler);

            /**
             * \brief Install an RTCP APP packet hook
             *
             * \details This function is called when an RTCP APP packet is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_app_hook(void (*hook)(uvgrtp::frame::rtcp_app_packet *));

            /**
             * \brief Install an RTCP APP packet hook
             *
             * \details This function is called when an RTCP APP packet is received
             *
             * \param app_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_app_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler);

            /// \cond DO_NOT_DOCUMENT
            // These have been replaced by functions with unique_ptr in them
            rtp_error_t install_sender_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler);
            rtp_error_t install_receiver_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler);
            rtp_error_t install_sdes_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler);
            rtp_error_t install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler);
            /// \endcond

            /**
             * \brief Install hook for one type of APP packets
             *
             * \details Each time the RR/SR is sent, all APP sending hooks call their respective functions to get the data
             * 
             * \param app_name name of the APP packet. Max 4 chars
             * \param app_sending the function to be called when hook fires
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If app_name is empty or longer that 4 characters or function pointer is empty
            */
            rtp_error_t install_send_app_hook(std::string app_name, std::function<std::unique_ptr<uint8_t[]>(uint8_t& subtype, uint32_t& payload_len)> app_sending_func);
            
            /**
             * \brief Remove all installed hooks for RTCP
             *
             * \details Removes all installed hooks so they can be readded in case of changes
             *
             * \retval RTP_OK on success
             */
            rtp_error_t remove_all_hooks();

            /**
             * \brief Remove a hook for sending APP packets
             *             *
             * \param app_name name of the APP packet hook. Max 4 chars
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE if hook with given app_name was not found
            */
            rtp_error_t remove_send_app_hook(std::string app_name);

            /* Return the latest RTCP packet received from participant of "ssrc"
             * Return nullptr if we haven't received this kind of packet or if "ssrc" doesn't exist
             *
             * NOTE: Caller is responsible for deallocating the memory */
            uvgrtp::frame::rtcp_sender_report* get_sender_packet(uint32_t ssrc);
            uvgrtp::frame::rtcp_receiver_report* get_receiver_packet(uint32_t ssrc);
            uvgrtp::frame::rtcp_sdes_packet* get_sdes_packet(uint32_t ssrc);
            uvgrtp::frame::rtcp_app_packet* get_app_packet(uint32_t ssrc);

        private:

            rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::shared_ptr<std::atomic<std::uint32_t>> ssrc, std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
                std::string cname, std::shared_ptr<uvgrtp::socketfactory> sfp, int rce_flags);
            rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::shared_ptr<std::atomic<std::uint32_t>> ssrc, std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
                std::string cname, std::shared_ptr<uvgrtp::socketfactory> sfp, std::shared_ptr<uvgrtp::srtcp> srtcp, int rce_flags);


            std::shared_ptr<rtcp_internal> pimpl_;
    };
}

namespace uvg_rtp = uvgrtp;
