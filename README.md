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

## Building and linking

See [Building](BUILDING.md) for instructions on how to build and use kvzRTP

## Examples

Please see [examples](examples/) directory for different kvzRTP examples

# Adding support for new media types

Adding support for new media types quite straight-forward:
* add the payload to util.hh's `RTP_FORMAT` list
* create files to src/formats/`format_name`.{cc, hh}
* create `namespace format_name` inside `namespace kvz_rtp`
* Add functions `push_frame()` and `frame_receiver()`
   * You need to implement all (de)fragmentation required by the media type

See src/formats/hevc.cc and src/formats/hevc.hh for help when in doubt.
