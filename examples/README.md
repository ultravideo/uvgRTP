# uvgRTP example codes

This directory contains a collection of commented and functioning examples that demonstrate the usage of uvgRTP. Currently, the API is very C-like and would benefit greatly from using more C++ features. Help with this would be greatly appreciated.

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

Next, you will use the context object to create session objects. Session object contains all the different media streams you are sending/receiving to/from single IP address. There are two options fort creating this, Specify only remote address (currently this also binds to ANY with each media_stream):

```
uvgrtp::session *sess = ctx.create_session("10.10.10.1");
```
or specify both remote and local addresses:

```
uvgrtp::session *sess = ctx.create_session("10.10.10.1", "10.10.10.2");
```

Hopefully in the future also only binding to local address and only sending will be supported. This is discussed in issue #83 and PRs are welcome to this issue (be careful not to invalidate current API).

### Create media_stream

To send/receive actual media, a media_stream object has to be created. The first parameter is the local port from which the sending happens and the second port is the port where the data is sent to (note that these are in the reverse order compared to creating the session). The third parameter specifies the RTP payload format which will be used for the outgoing and incoming data. The last parameter holds the flags that can be used to modify the behavior of uvgRTP in regards to this media_stream. 

```
uvgrtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);
```

The encryption can be enabled here bug specifying `RCE_SRTP| RCE_SRTP_KMNGMNT_ZRTP` or `RCE_SRTP | RCE_SRTP_KMNGMNT_USER` in the last parameter. The `RCE_SRTP_KMNGMNT_USER` requires calling `add_srtp_ctx(key, salt)` for the created media_stream after creation. These flags start with prefix `RCE_` and the explanations can be found in [docs folder](../docs). Other useful flags include `RCE_RTCP` for enabling RTCP and `RCE_H26X_PREPEND_SC` for prepending start codes which are needed for playback of the stream (currently there is a bug in this: #96).

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

## Basic RTP examples

[How to create a simple RTP sender](sending.cc) (Pair with one of the receiver examples)

[How to create a simple RTP receiver (hooking)](receiving_hook.cc)

NOTE: The hook should not be used for extensive media processing. It is meant to be used as an interface between application and library where uvgRTP hands off the RTP frames to an application thread.

[How to create a simple RTP receiver (polling)](receiving_poll.cc)

## Advanced RTP examples

[How to modify uvgRTP behavior](configuration.cc)

[How to fragment generic media types](sending_generic.cc)

[How to enable UDP hole punching](binding.cc)

[How to use custom timestamps correctly](custom_timestamps.cc)

## RTCP example

[How to use RTCP instance (hooking)](rtcp_hook.cc)

## Encryption examples

[How to use SRTP with ZRTP](srtp_zrtp.cc)

[How to use multi-stream SRTP with ZRTP](zrtp_multistream.cc)

[How to use SRTP with user-managed keys](srtp_user.cc)
