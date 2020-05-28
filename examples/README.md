# uvgRTP example codes

This directory contains a collection of simple examples that demonstrate how to use uvgRTP

## Available examples

We provide several simple and thoroughly commented examples on how to use uvgRTP.

[How to create a simple RTP sender](sending.cc)

[How to use fragmented input with uvgRTP \(HEVC slices\)](sending_fragmented.cc)

[How to fragment generic media types](sending_generic.cc)

[How to configure uvgRTP to send high-quality video](configuration.cc)

[How to create a simple RTP receiver (hooking)](receiving_hook.cc)

NOTE: The hook should **not** be used for media processing. It should be rather used as interface between application and library where the frame handout happens.

[How to create a simple RTP receiver (polling)](receiving_poll.cc)

[How to create an RTCP instance (polling)](rtcp_poll.cc)

[How to create an RTCP instance (hoooking)](rtcp_hook.cc)

[How to use SRTP with ZRTP](srtp_zrtp.cc)

[How to use SRTP with user-managed keys](srtp_user.cc)

### Memory ownership/deallocation

If you have not enabled the system call dispatcher, you don't need to worry about these

[Method 1, unique_ptr](deallocation_1.cc)

[Method 2, copying](deallocation_2.cc)

[Method 3, deallocation hook](deallocation_3.cc)
