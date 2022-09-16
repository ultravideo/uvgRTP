# uvgRTP

uvgRTP is an *Real-Time Transport Protocol (RTP)* library written in C++ with a focus on simple to use and high-efficiency media delivery over the Internet. It features an intuitive and easy-to-use *Application Programming Interface (API)*, built-in support for transporting *Versatile Video Coding (VVC)*, *High Efficiency Video Coding (HEVC)*, *Advanced Video Coding (AVC)* encoded video and Opus encoded audio. uvgRTP also supports *End-to-End Encrypted (E2EE)* media delivery using the combination of *Secure RTP (SRTP)* and ZRTP. According to [our measurements](https://researchportal.tuni.fi/en/publications/open-source-rtp-library-for-high-speed-4k-hevc-video-streaming) uvgRTP is able to reach a goodput of 600 MB/s (4K at 700fps) for HEVC stream when measured in LAN. The CPU usage is relative to the goodput value, and therefore smaller streams have a very small CPU usage.

uvgRTP is licensed under the permissive BSD 2-Clause License. This cross-platform library can be run on both Linux and Windows operating systems. Mac OS is also supported, but the support relies on community contributions. For SRTP/ZRTP support, uvgRTP uses [Crypto++ library](https://www.cryptopp.com/). 

Currently supported specifications:
   * [RFC 3550: RTP: A Transport Protocol for Real-Time Applications](https://tools.ietf.org/html/rfc3550)
   * [RFC 7798: RTP Payload Format for High Efficiency Video Coding (HEVC)](https://tools.ietf.org/html/rfc7798)
   * [RFC 6184: RTP Payload Format for H.264 Video](https://tools.ietf.org/html/rfc6184)
   * [RFC 7587: RTP Payload Format for the Opus Speech and Audio Codec](https://tools.ietf.org/html/rfc7587)
   * [RFC 3711: The Secure Real-time Transport Protocol (SRTP)](https://tools.ietf.org/html/rfc3711)
   * [RFC 6189: ZRTP: Media Path Key Agreement for Unicast Secure RTP](https://tools.ietf.org/html/rfc6189)
   * [Draft: RTP Payload Format for Versatile Video Coding (VVC)](https://tools.ietf.org/html/draft-ietf-avtcore-rtp-vvc-08)

## Notable features

* AVC/HEVC/VVC video streaming, including packetization
* Ready support for many formats which don't need packetization, including Opus
* Delivery encryption with SRTP
* Encryption key negotiation with ZRTP
* UDP firewall hole punching
* Simple to use API
* Working examples
* Permissive license

## Tutorial

You can either include files individually from [include](../include) folder or use lib.hh to include all necessary files with one line:

```
#include <uvgrtp/lib.hh>
```

### Create context

When using uvgRTP, you must always first create the context object:

```
uvgrtp::context ctx;
```
### Create session

Next, you will use the context object to create session objects. Session object contains all the different media streams you are sending/receiving to/from single IP address. Broadcast addresses should also work. There are two options fort creating this, Specify only remote address (currently this also binds to ANY with each media_stream):

```
uvgrtp::session *sess = ctx.create_session("10.10.10.2");
```
or specify both remote and local addresses:

```
uvgrtp::session *sess = ctx.create_session("10.10.10.2", "10.10.10.3");
```

Hopefully in the future also only binding to local address and only sending will be supported. This is discussed in issue #83 and PRs are welcome to this issue (be careful not to invalidate current API).

### Create media_stream

To send/receive actual media, a media_stream object has to be created. The first parameter is the local port from which the sending happens and the second port is the port where the data is sent to (note that these are in the reverse order compared to creating the session). The third parameter specifies the RTP payload format which will be used for the outgoing and incoming data. The last parameter holds the flags that can be used to modify the behavior of uvgRTP in regards to this media_stream. 

```
uvgrtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);
```

The encryption can be enabled here bug specifying `RCE_SRTP| RCE_SRTP_KMNGMNT_ZRTP` or `RCE_SRTP | RCE_SRTP_KMNGMNT_USER` in the last parameter. The `RCE_SRTP_KMNGMNT_USER` requires calling `add_srtp_ctx(key, salt)` for the created media_stream after creation. These flags start with prefix `RCE_` and the explanations can be found in [docs folder](../docs). Other useful flags include `RCE_RTCP` for enabling RTCP and `RCE_H26X_PREPEND_SC` for prepending start codes which are needed for decoding of an H26x stream.

### Configure media_stream (optional)

Some of the media_stream functionality can be configured after the stream has been created:
```
strm->configure_ctx(RCC_MTU_SIZE, 2312);
```

The flags start with prefix `RCC_` and the rest of the flags can be found in the [docs folder](../docs). Also, see [configuration example](configuration.cc) for more details.

### Sending data

Sending can be done by simple calling push_frame()-function on created media_stream:

```
strm->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
```
See [sending example](sending.cc) for more details.

### Receiving data

There are two alternatives to receiving data. Using pull_frame()-function:
```
auto frame = strm->pull_frame();
```

or function callback based approach (I would recommend this to minimize latency):

```
strm->install_receive_hook(nullptr, rtp_receive_hook);
```

If you use classes, you can give a pointer to your class in the first parameter and call it in you callback function (an std::function API would be nice, but does not exist yet). In both versions, the user will be responsible for releasing the memory.

### Cleanup

Cleanup can be dune with following functions:
```
sess->destroy_stream(strm);
ctx.destroy_session(sess);
```

### Simple example (non-working)

```
#include <uvgrtp/lib.hh>

/* g++ main.cc -luvgrtp -lpthread && ./a.out */

int main(void)
{
    uvgrtp::context ctx;
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    uvgrtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);

    strm->configure_ctx(RCC_MTU_SIZE, 2312);

    char *message  = (char *)"Hello, world!";
    size_t msg_len = strlen(message) + 1;

    for (;;) {
        strm->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
        auto frame = strm->pull_frame();
        fprintf(stderr, "Message: '%s'\n", frame->payload);
        uvgrtp::frame::dealloc_frame(frame);
    }
}
```
