# Documentation for uvgRTP

This file provides documentation of uvgRTP's usage and public API.

## Architecture

![](arch.png)

The top-level object for uvgRTP is the `uvgrtp::context` object. It is used to create RTP sessions
that are bound to certain IP addresses and to provide CNAME namespace isolation if that is
required by the application. Most of the time only one context object is enough per application.

RTP sessions, `uvgrtp::session` objects, are allocated from `uvgrtp::context`. For each remote IP address, a session object should be created. uvgRTP supports UDP holepunching so during session creation a source IP address can be specified and uvgRTP binds itself to that address before it starts streaming.

Each session contains 1..n `uvgrtp::media_stream` objects. These objects are bidirectional streams, i.e. you use the same object to send and receive RTP frames. The object can be used as unidirectional stream too, a user then just doesn't either send or receive RTP frames using the object. Each `uvgrtp::media_stream` object contains source and destination ports, media format for the stream and a collection of context configuration flags that enable, for example, SRTP or RTCP.

## Public API

The public API for uvgRTP is very short. Functions not listed in the public API should not be called
by an application using uvgRTP as they are for internal usage only. All of the features are provided
as either built-in or they are enabled/modified through context configuration that is specified below.

Read the documentation for uvgRTP's public API [here](html/INDEX.md)

## Supported media formats

Currently uvgRTP has a built-in support for the following media formats:
* AVC
* HEVC
* VVC
* Opus

uvgRTP also features a generic media frame API that can be used to fragment and send any media format,
see [this example code](examples/sending_generic.cc) for more details. Fragmentation of generic media formats is a uvgRTP exclusive feature and does not work with other RTP libraries so please use it only if you are using uvgRTP for both sending and receiving.

## Context configuration

`RCE_*` flags are used to enable/disable functionality of an **individual** `uvgrtp::media_stream` object. Table below lists all supported flags and what they enable/disable.
`RCE_*` flags are passed to `create_stream()` as the last parameter and they can be OR'ed together
```
session->create_stream(..., RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_SRTP_NULL_CIPHER);
```

| Flag | Explanation |
| ---- |:----------:|
| RCE_SRTP | Enable SRTP, must be coupled with either RCE_SRTP_KMNGMNT_ZRTP or RCE_SRTP_KMNGMNT_USER |
| RCE_SRTP_KMNGMNT_ZRTP | Use ZRTP to manage keys (see section SRTP for more details) |
| RCE_SRTP_KMNGMNT_USER | Let user manage keys (see section SRTP for more details) |
| RCE_NO_H26X_INTRA_DELAY | When uvgRTP is receiving H26X stream, as an attempt to improve QoS, it will set frame delay for intra frames to be the same as intra period.  What this means is that if the regular timer expires for frame (100 ms) and the frame type is intra, uvgRTP will not drop the frame but will continue receiving packets in hopes that all the packets of the intra frame will be received and the frame can be returned to user. During this period, when the intra frame is deemed to be late and incomplete, uvgRTP will drop all inter frames until a) all the packets of late intra frame are received or b) a new intra frame is received This behaviour should reduce the number of gray screens during video decoding but might cause the video stream to freeze for a while which is subjectively lesser of two evils This behavior can be disabled with `RCE_NO_H26X_INTRA_DELAY` If this flag is given, uvgRTP treats all frame types equally and drops all frames that are late |
| RCE_FRAGMENT_GENERIC | Fragment generic media frames into RTP packets of 1500 bytes (size is configurable, see RCC_MTU_SIZE) |
| RCE_SRTP_INPLACE_ENCRYPTION | Perform ciphering directly on the input frame. Saves a memory copy but makes the input frame unusable for the application |
| RCE_NO_SYSTEM_CALL_CLUSTERING | Disable System Call Clustering, see the publication for more details |
| RCE_SRTP_NULL_CIPHER | Use NULL cipher for SRTP, i.e. do not encrypt packets |
| RCE_SRTP_AUTHENTICATE_RTP | Add RTP authentication tag to each RTP packet and verify authenticity of each received packet before they are returned to the user |
| RCE_SRTP_REPLAY_PROTECTION | Monitor and reject replayed RTP packets |
| RCE_RTCP | Enable RTCP |
| RCE_H26X_PREPEND_SC | Prepend a 4-byte start code (0x00000001) before each NAL unit |
| RCE_HOLEPUNCH_KEEPALIVE | Keep the hole made in the firewall open in case the streaming is unidirectional. If holepunching has been enabled during session creation and this flag is given to `create_stream()` and uvgRTP notices that the application has not sent any data in a while (unidirectionality), it sends a small UDP datagram to the remote participant to keep the connection open |

`RCC_*` flags are used to modify the default values used by uvgRTP. Table below lists all supported flags and what they modify.

| Flag | Explanation | Default |
| ---- |:----------:|:----------:|
| RCC_UDP_RCV_BUF_SIZE | Specify UDP receive buffer size | 4 MB |
| RCC_UDP_SND_BUF_SIZE | Specify UDP send buffer size | 4 MB
| RCC_PKT_MAX_DELAY | How many milliseconds is each frame waited until they're dropped (for fragmented frames only) | 100 ms |
| RCC_DYN_PAYLOAD_TYPE | Override uvgRTP's payload type used in RTP headers | Format-specific, see `include/util.hh` |
| RCC_MTU_SIZE | Set a maximum value for the Ethernet frame size assumed by uvgRTP (for enabling, for example, jumbo frame support) | 1500 bytes |

Configuration done using `RCC_*` flags are done by calling `configure_ctx()` with a flag and a value

```
stream->configure_ctx(RCC_PKT_MAX_DELAY, 150);
```

## SRTP

uvgRTP provides two ways for an application to deal with SRTP key-management: ZRTP or user-managed.
Out of the two, ZRTP is simpler but requires message exchanging before media encryption can take place
whereas for user-managed keys the calling application must provide an encryption key and a salt.

### ZRTP-based SRTP

uvgRTP supports Diffie-Hellman and Multistream modes of ZRTP. The mode selection is done transparently
and the only thing an application must do is to provide `RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP` flag combination
to `create_stream()`. See [ZRTP Multistream example](examples/zrtp_multistream.cc) for more details.

### User-managed SRTP

The second way of handling key-management of SRTP is to do it yourself. uvgRTP supports 128-bit keys
and and 112-bit salts which must be given to the `uvgrtp::media_stream` object using `add_srtp_ctx()` right after
`create_stream()` has been called. All calls that try to modify or use the stream
(other than `add_srtp_ctx()`) will fail with `RTP_NOT_INITIALIZED`.
See [this example code](examples/srtp_user.cc) for more details.
