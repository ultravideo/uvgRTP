# uvgRTP

uvgRTP is an *Real-Time Transport Protocol (RTP)* library written in C++ with a focus on simple to use and high-efficiency media delivery over the internet. It features an intuitive and easy-to-use *Application Programming Interface (API)*, built-in support for transporting *Versitile Video Coding (VVC)*, *High Efficiency Video Coding (HEVC)*, *Advanced Video Coding (AVC)* encoded video and Opus encoded audio. uvgRTP also supports *End-to-End Encrypted (E2EE)* media delivery using the combination of *Secure RTP (SRTP)* and ZRTP. According to [our measurements](https://researchportal.tuni.fi/en/publications/open-source-rtp-library-for-high-speed-4k-hevc-video-streaming) uvgRTP is able to reach a goodput of 600 MB/s (4K at 700fps) for HEVC stream when measured in LAN. The CPU usage is relative to the goodput value, and therefore smaller streams have a very small CPU usage.

uvgRTP is licensed under the permissive BSD 2-Clause License. For SRTP/ZRTP support, uvgRTP uses [Crypto++ library](https://www.cryptopp.com/).

Currently supported specifications:
   * [RFC 3350: RTP: A Transport Protocol for Real-Time Applications](https://tools.ietf.org/html/rfc3550)
   * [RFC 7798: RTP Payload Format for High Efficiency Video Coding (HEVC)](https://tools.ietf.org/html/rfc7798)
   * [RFC 6184: RTP Payload Format for H.264 Video](https://tools.ietf.org/html/rfc6184)
   * [RFC 7587: RTP Payload Format for the Opus Speech and Audio Codec](https://tools.ietf.org/html/rfc7587)
   * [RFC 3711: The Secure Real-time Transport Protocol (SRTP)](https://tools.ietf.org/html/rfc3711)
   * [RFC 6189: ZRTP: Media Path Key Agreement for Unicast Secure RTP](https://tools.ietf.org/html/rfc6189)
   * [Draft: RTP Payload Format for Versatile Video Coding (VVC)](https://tools.ietf.org/html/draft-ietf-avtcore-rtp-vvc-08)

The original version of uvgRTP is based on Marko Viitanen's [fRTPlib library](https://github.com/fador/fRTPlib).

## Notable features

* Built-in support for:
    * AVC/HEVC/VVC video streaming
    * Opus audio streaming
    * Delivery encryption with SRTP/ZRTP
* Generic interface for custom media types
* UDP hole punching
* Simple to use API
* Permissive license

## Building and linking

See [BUILDING.md](BUILDING.md) for instructions on how to build and use uvgRTP

## Documentation and examples

See [documentation](docs/README.md) and [examples](docs/examples) to get a better understanding of uvgRTP

## Contributing

We warmly welcome any contributions to the project. If you are thinking about submitting a pull request, please read [CONTRIBUTING.md](CONTRIBUTING.md) before proceeding.

## Paper

If you use uvgRTP in your research, please cite the following [paper](https://researchportal.tuni.fi/en/publications/open-source-rtp-library-for-high-speed-4k-hevc-video-streaming):

```A. Altonen, J. Räsänen, J. Laitinen, M. Viitanen, and J. Vanne, “Open-Source RTP Library for High-Speed 4K HEVC Video Streaming”, in Proc. IEEE Int. Workshop on Multimedia Signal Processing, Tampere, Finland, Sept. 2020.```
