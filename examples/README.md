# uvgRTP example codes

This directory contains a collection of commented and functioning examples that demonstrate the usage of uvgRTP.

## Building the examples

Compile the library according to instructions in [BUILDING.md](../BUILDING.md).
Follow either Visual Studio or Linux compilation instructions below:

### Visual Studio compilation

Navigate to `uvgRTP/build/examples` and open the `uvgrtp_examples.sln` with Visual Studio, through which you can build the individual example programs.

### Linux and MinGW compilation

Navigate to `uvgRTP/build/examples` and compile the programs with the `make` command.

## Basic RTP examples

[How to create a simple RTP sender](sending.cc) (Pair with one of the receiver examples)

[How to create a simple RTP receiver (hooking)](receiving_hook.cc)

NOTE: The hook should not be used for extensive media processing. It is meant to be used as an interface between application and library where uvgRTP hands off the RTP frames to an application thread.

[How to create a simple RTP receiver (polling)](receiving_poll.cc)

## Visual Volumetric Video-based Coding (V3C) streaming

To demonstrate V3C streaming, uvgRTP comes with example V3C sender and V3C receiver programs, found in `uvgRTP/build/examples`. Below are simple step-by-step instructions to run these programs:

1. Build the library according to instructions in [BUILDING.md](../BUILDING.md). Crypto++ is not needed.
3. Download a test sequence from [here](https://ultravideo.fi/uvgRTP_example_sequence_longdress.vpcc).
4. Set the PATH variable in the [v3c_sender](v3c_sender.cc#L20) and [v3c_receiver](v3c_receiver.cc#L49) to the path of the test sequence.
5. Build the `v3c_sender` and `v3c_receiver` applications according to building instructions on the top of this page.
6. Start the `v3c_receiver` program.
7. Start the `v3c_sender` program. The program will parse the test sequence for transmission and send it to the receiver.

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