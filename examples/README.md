# uvgRTP example codes

This directory contains a collection of simple and thoroughly commented examples that demonstrate how to use uvgRTP

Below is a very simple example usage of uvgRTP:

```
#include <uvgrtp/lib.hh>

/* g++ main.cc -luvgrtp -lpthread && ./a.out */

int main(void)
{
    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    uvg_rtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);

    char *message  = (char *)"Hello, world!";
    size_t msg_len = strlen(message);

    for (;;) {
        strm->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
        auto frame = strm->pull_frame();
        fprintf(stderr, "Message: '%s'\n", frame->payload);
        uvg_rtp::frame::dealloc_frame(frame);
    }
}
```

## Basic RTP functionality

[How to create a simple RTP sender](sending.cc)

[How to create a simple RTP receiver (hooking)](receiving_hook.cc)

NOTE: The hook should **not** be used for media processing. It should be used as interface between application and library where the frame handout happens.

[How to create a simple RTP receiver (polling)](receiving_poll.cc)

## Advanced RTP functionality

[How to fragment generic media types](sending_generic.cc)

[How to configure uvgRTP to send high-quality video](configuration.cc)

[How to enable UDP hole punching](binding.cc)

[How to use custom timestamps correctly](custom_timestamps.cc)

## RTCP

[How to use RTCP instance (hooking)](rtcp_hook.cc)

## Security

[How to use SRTP with ZRTP](srtp_zrtp.cc)

[How to use multi-stream SRTP with ZRTP](zrtp_multistream.cc)

[How to use SRTP with user-managed keys](srtp_user.cc)
