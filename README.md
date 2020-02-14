# kvzRTP

kvzRTP is a thread-safe, easy-to-use RTP library written in C++ especially designed for high-speed multimedia applications. In ideal conditions it's able to receive up to ~610 MB/s goodput for HEVC stream. It features a very intuitive and easy-to-use API.

kvzRTP is licensed under the permissive BSD 2-Clause License

For SRTP/ZRTP support, kvzRTP uses [Crypto++](https://www.cryptopp.com/)

Supported specifications:
   * [RFC 3350: RTP: A Transport Protocol for Real-Time Applications](https://tools.ietf.org/html/rfc3550)
   * [RFC 7798: RTP Payload Format for High Efficiency Video Coding (HEVC)](https://tools.ietf.org/html/rfc7798)
   * [RFC 7587: RTP Payload Format for the Opus Speech and Audio Codec](https://tools.ietf.org/html/rfc7587)
   * [RFC 3711: The Secure Real-time Transport Protocol (SRTP)](https://tools.ietf.org/html/rfc3711)
   * [RFC 6189: ZRTP: Media Path Key Agreement for Unicast Secure RTP](https://tools.ietf.org/html/rfc6189)

Based on Marko Viitanen's [fRTPlib](https://github.com/fador/fRTPlib)

## Building

```
make -j4
sudo make install
```

You can also use QtCreator to build the library. The library must be built using a 64-bit compiler!

#### SRTP/ZRTP support

If you want SRTP/ZRTP support, you must compile kvzRTP with `-D__RTP_CRYPTO__`

## Linking

#### Linux
`-lkvzrtp -lpthread`

#### Windows
`-L<path to library folder> -lkvzrtp -lpthread -lwsock32 -lws2_32`

#### SRTP/ZRTP support

If you want SRTP/ZRTP support, you must compile [Crypto++](https://www.cryptopp.com/) and link it as a static library

#### Linux
`-lkvzrtp -lpthread -lcryptopp`

#### Windows
`-L<path to library folder> -lkvzrtp -lpthread -lcryptopp -lwsock32 -lws2_32`

## Examples

We provide several simple and thoroughly commented examples on how to use kvzRTP, please see:

[How to create a simple RTP sender](examples/simple/rtp/sending.cc)

[How to use fragmented input with kvzRTP \(HEVC slices\)](examples/simple/rtp/sending_fragmented.cc)

[How to create a simple RTP receiver (hooking)](examples/simple/rtp/receiving_hook.cc)

NOTE: The hook should **not** be used for media processing. It should be rather used as interface between application and library where the frame handout happens.

[How to create a simple RTP receiver (polling)](examples/simple/rtp/receiving_poll.cc)

[How to create an RTCP instance (polling)](examples/simple/rtcp/rtcp_poll.cc)

[How to create an RTCP instance (hoooking)](examples/simple/rtcp/rtcp_hook.cc)

## Configuration

By default, kvzRTP does not require any configuration but if the participants are sending high-quality video, some things must be configured

[How to configure RTP sender for high-quality video](examples/simple/rtp/send_hq.cc)

[How to configure RTP receiver for high-quality video](examples/simple/rtp/recv_hq.cc)

[How to configure SRTP with ZRTP](examples/simple/rtp/srtp_zrtp.cc)

[How to configure SRTP with user-managed keys](examples/simple/rtp/srtp_user.cc)

### Memory ownership/deallocation

If you have not enabled the system call dispatcher, you don't need to worry about these

[Method 1, unique_ptr](examples/simple/rtp/deallocation_1.cc)

[Method 2, copying](examples/simple/rtp/deallocation_2.cc)

[Method 3, deallocation hook](examples/simple/rtp/deallocation_3.cc)

## Defines

Use `__RTP_SILENT__` to disable all prints

Use `__RTP_CRYPTO__` to enable SRTP/ZRTP and crypto routines

Use `NDEBUG` to disable `LOG_DEBUG` which is the most verbose level of logging

# Adding support for new media types

Adding support for new media types quite straight-forward:
* add the payload to util.hh's `RTP_FORMAT` list
* create files to src/formats/`format_name`.{cc, hh}
* create `namespace format_name` inside `namespace kvz_rtp`
* Add functions `push_frame()` and `frame_receiver()`
   * You need to implement all (de)fragmentation required by the media type

See src/formats/hevc.cc and src/formats/hevc.hh for help when in doubt.
