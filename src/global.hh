#pragma once



namespace uvgrtp {
    constexpr uint8_t IPV4_HDR_SIZE = 20;
    constexpr uint8_t IPV6_HDR_SIZE = 40; // TODO: Ipv6 support
    constexpr uint8_t UDP_HDR_SIZE = 8;
    constexpr uint8_t RTP_HDR_SIZE = 12;

    /* the default MTU size for ethernet, can be adjusted with rcc flags.
     * The default is assumed to be 1500 minus 8 bytes reserved for additional protocol headers- */
    constexpr uint16_t DEFAULT_MTU_SIZE = 1492;

    // here we ignore ethernet frame header size since it is not included in MTU
    constexpr uint16_t MAX_IPV4_PAYLOAD = DEFAULT_MTU_SIZE - IPV4_HDR_SIZE - UDP_HDR_SIZE;
    constexpr uint16_t MAX_IPV6_PAYLOAD = DEFAULT_MTU_SIZE - IPV6_HDR_SIZE - UDP_HDR_SIZE;

    constexpr uint16_t MAX_IPV4_MEDIA_PAYLOAD = MAX_IPV4_PAYLOAD - RTP_HDR_SIZE;
    constexpr uint16_t MAX_IPV6_MEDIA_PAYLOAD = MAX_IPV6_PAYLOAD - RTP_HDR_SIZE;

    constexpr int PKT_MAX_DELAY_MS = 500;
}

