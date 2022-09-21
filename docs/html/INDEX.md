# uvgRTP

uvgRTP is an *Real-Time Transport Protocol (RTP)* library written in C++ with a focus on simple to use and high-efficiency media delivery over the Internet. It features an intuitive and easy-to-use *Application Programming Interface (API)*, built-in support for transporting *Versatile Video Coding (VVC)*, *High Efficiency Video Coding (HEVC)*, *Advanced Video Coding (AVC)* encoded video and Opus encoded audio. uvgRTP also supports *End-to-End Encrypted (E2EE)* media delivery using the combination of *Secure RTP (SRTP)* and ZRTP. According to [our measurements](https://researchportal.tuni.fi/en/publications/open-source-rtp-library-for-high-speed-4k-hevc-video-streaming) uvgRTP is able to reach a goodput of 600 MB/s (4K at 700fps) for HEVC stream when measured in LAN. The CPU usage is relative to the goodput value, and therefore smaller streams have a very small CPU usage.

uvgRTP is licensed under the permissive BSD 2-Clause License. This cross-platform library can be run on both Linux and Windows operating systems. Mac OS is also supported, but the support relies on community contributions. For SRTP/ZRTP support, uvgRTP uses [Crypto++ library](https://www.cryptopp.com/). 

Currently supported specifications:
   * [RFC 3550: RTP: A Transport Protocol for Real-Time Applications](https://tools.ietf.org/html/rfc3550)
   * [RFC 3551: RTP Profile for Audio and Video Conferences](https://tools.ietf.org/html/rfc3551)
   * [RFC 6184: RTP Payload Format for H.264 Video](https://tools.ietf.org/html/rfc6184)
   * [RFC 7798: RTP Payload Format for High Efficiency Video Coding (HEVC)](https://tools.ietf.org/html/rfc7798)
   * [Draft: RTP Payload Format for Versatile Video Coding (VVC)](https://tools.ietf.org/html/draft-ietf-avtcore-rtp-vvc-18)
   * [RFC 7587: RTP Payload Format for the Opus Speech and Audio Codec](https://tools.ietf.org/html/rfc7587)
   * [RFC 3711: The Secure Real-time Transport Protocol (SRTP)](https://tools.ietf.org/html/rfc3711)
   * [RFC 6189: ZRTP: Media Path Key Agreement for Unicast Secure RTP](https://tools.ietf.org/html/rfc6189)

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

You can either include files individually from the include-folder or use lib.hh to include all necessary files with one line:

```
#include <uvgrtp/lib.hh>
```

### Step 1: Create context

When using uvgRTP, you must always first create the uvgrtp::context object:

```
uvgrtp::context ctx;
```
### Step 2: Create session

Next, you will use the uvgrtp::context object to create uvgrtp::session objects. The uvgrtp::session object contains all media streams you are sending/receiving to/from single IP address. Broadcast addresses should also work. There are two options for creating this: 1) specify one address, role of which can be determined with RCE_SEND_ONLY or RCE_RECEIVE_ONLY flag later:

```
uvgrtp::session *sess = ctx.create_session("10.10.10.2");
```
or 2) specify both remote and local addresses:

```
uvgrtp::session *sess = ctx.create_session("10.10.10.2", "10.10.10.3");
```

### Step 3: Create media_stream

To send/receive actual media, a uvgrtp::media_stream object has to be created. The first parameter is the local port from which the sending happens and the second port is the port where the data is sent to (note that these are in the reverse order compared to creating the session). The third parameter specifies the RTP payload format which will be used for the outgoing and incoming data. The last parameter holds the flags that can be used to modify the behavior of created uvgrtp::media_stream. The flags can be combined using bitwise OR-operation(|). These flags start with prefix `RCE_` and the explanations can be found in docs folder of repository. RTCP can be enabled with `RCE_RTCP`-flag.

```
uvgrtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RCE_NO_FLAGS);
```

One port version of this also exists, to be used with RCE_SEND_ONLY and RCE_RECEIVE_ONLY flags:
```
uvgrtp::media_stream *strm = sess->create_stream(8888, RTP_FORMAT_GENERIC, RCE_RECEIVE_ONLY);
```

### Step 3.1: Encryption (optional)

The encryption can be enabled by specifying `RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP` or `RCE_SRTP | RCE_SRTP_KMNGMNT_USER` in the flags parameter of create_stream. The `RCE_SRTP_KMNGMNT_USER` requires calling `add_srtp_ctx(key, salt)` for the created uvgrtp::media_stream. 

### Step 3.2: Configure media_stream (optional)

Some of the uvgrtp::media_stream functionality can be configured after the stream has been created:
```
strm->configure_ctx(RCC_MTU_SIZE, 2312);
```

The flags start with prefix `RCC_` and the rest of the flags can be found in the docs folder. Also, see the configuration example for more details.

### Step 4: Sending data

Sending can be done by simple calling push_frame()-function on created uvgrtp::media_stream:

```
strm->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
```
See the sending example for more details. uvgRTP does not take ownership of the memory unless the data is provided with std::unique_ptr.

### Step 5: Receiving data

There are two alternatives to receiving data. Using pull_frame()-function:
```
auto frame = strm->pull_frame();
```

or function callback based approach (I would recommend this to minimize latency):

```
strm->install_receive_hook(nullptr, rtp_receive_hook);
```

If you use classes, you can give a pointer to your class in the first parameter and call it in your callback function (an std::function API does not exist yet). In both versions of receiving, the user will be responsible for releasing the memory with the following function:
```
uvgrtp::frame::dealloc_frame(frame);
```

### Step 6: Cleanup

Cleanup can be done with following functions:
```
sess->destroy_stream(strm);
ctx.destroy_session(sess);
```

### Simple sending example (non-working)

```
#include <uvgrtp/lib.hh>

/* g++ main.cc -luvgrtp -lpthread && ./a.out */

int main(void)
{
    uvgrtp::context ctx;
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    uvgrtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RCE_NO_FLAGS);

    strm->configure_ctx(RCC_MTU_SIZE, 2312);

    char *message  = (char *)"Hello, world!";
    size_t msg_len = strlen(message) + 1;

    for (;;) {
        strm->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
        auto frame = strm->pull_frame();
        fprintf(stderr, "Message: '%s'\n", frame->payload);
        uvgrtp::frame::dealloc_frame(frame);
    }

    sess->destroy_stream(strm);
    ctx.destroy_session(sess);
}
```