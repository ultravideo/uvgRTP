# uvgRTP example codes

This directory contains a collection of simple and thoroughly commented examples that demonstrate how to use uvgRTP

## Basic RTP functionality

[How to create a simple RTP sender](sending.cc)

[How to create a simple RTP receiver (hooking)](receiving_hook.cc)

NOTE: The hook should **not** be used for media processing. It should be used as interface between application and library where the frame handout happens.

[How to create a simple RTP receiver (polling)](receiving_poll.cc)

## Advanced RTP functionality

[How to fragment generic media types](sending_generic.cc)

[How to configure uvgRTP to send high-quality video](configuration.cc)

## RTCP

[How to use RTCP instance (polling)](rtcp_poll.cc)

[How to use RTCP instance (hooking)](rtcp_hook.cc)

## Security

[How to use SRTP with ZRTP](srtp_zrtp.cc)

[How to use SRTP with user-managed keys](srtp_user.cc)
