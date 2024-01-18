# uvgRTP example codes

This directory contains a collection of commented and functioning examples that demonstrate the usage of uvgRTP.

## Basic RTP examples

[How to create a simple RTP sender](sending.cc) (Pair with one of the receiver examples)

[How to create a simple RTP receiver (hooking)](receiving_hook.cc)

NOTE: The hook should not be used for extensive media processing. It is meant to be used as an interface between application and library where uvgRTP hands off the RTP frames to an application thread.

[How to create a simple RTP receiver (polling)](receiving_poll.cc)

## Visual Volumetric Video-based Coding (V3C) streaming

Using RTP for transmission of V3C bitstreams such as V-PCC or MIV files, stored in the *sample stream format* requires:
1. Parsing the bitstream into NAL units for RTP transmission
2. Reconstructing the V3C bitstream from received NAL units

Included below are example implementations of these processes and the session structure of uvgRTP for V-PCC transmission. You can download a suitable V-PCC test sequence [here](https://ultravideo.fi/uvgRTP_example_sequence_longdress.vpcc).

[How to parse and transmit a V-PCC bitstream](v3c_sender.cc)

[How to receive and reconstruct a V-PCC bitstream](v3c_receiver.cc)

## RTCP example

[How to use RTCP instance (hooking)](rtcp_hook.cc)

## Encryption examples

Make sure you have checked the [build instructions](../BUILDING.md#linking-uvgrtp-and-crypto-to-an-application) if you want to build the encryption examples with Visual Studio.

[How to use SRTP with ZRTP](srtp_zrtp.cc)

[How to use multi-stream SRTP with ZRTP](zrtp_multistream.cc)

[How to use SRTP with user-managed keys](srtp_user.cc)

## Advanced RTP examples

[How to modify uvgRTP behavior](configuration.cc)

[How to fragment generic media types](sending_generic.cc)

[How to enable UDP hole punching](binding.cc)

[How to use custom timestamps correctly](custom_timestamps.cc)