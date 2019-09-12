# kvzRTP

Based on Marko Viitanen's [fRTPlib](https://github.com/fador/fRTPlib)

# Building

Linux
```
make -j8
sudo make install
```

You can also use QtCreator to build the library. The library must be built using a 64-bit compiler!

# Usage

The library should be linked as a static library to your program.

Linux

`-L<path to library folder> -lkvzrtp -lpthread`

Windows

`-L<path to library folder> -lkvzrtp -lpthread -lwsock32 -lws2_32`

# Defines

Use  `__RTP_SILENT__` to disable all prints

Ue `__RTP_USE_OPTIMISTIC_RECEIVER__` to enable the optimistic fragment receiver
* See src/formats/hevc.cc for more details
* NOTE: To use the receiver, you must be sure that no individual packet is larger than MTU (1500 bytes)

Use `__RTP_USE_PROBATION_ZONE__` to enable the probation zone allocation for RTP frames
* See src/frame.hh for more details
* NOTE: Probation zone is enabled only if optimistic fragment receiver is enabled

Use `NDEBUG` to disable `LOG_DEBUG` which is the most verbose level of logging

# Return values

There are two classes of return values: positive and negative

Negative return value means that some condition that the library can't handle happened and the function failed (out of memory, invalid parameters etc.)

Positive return value means that the function call didn't succeed but didn't fail completely either. Examples of these would be polling a socket timeouts when listening to incoming RTCP status reports (`RTP_INTERRUPTED`) or when `process_hevc_frame()` returns `RTP_NOT_READY` when the full frame has not been received.

When an operation succeeds, `RTP_OK` is returned

# API

## Sending data

Sending data is a simple as calling `writer->push_frame()`, see examples/sending/hevc_sender.cc.

## Receiving data

Reading frames can be done using two different ways: polling frames or installing a receive hook.

Polling frames is a blocking operation and a separate thread should be created for it.
examples/receiving/recv_example_1.cc shows how the polling approach works.

The second way to receive frames is to install a receive hook and when an RTP frame is received, this receive hook is called. Creating separate thread for reading data is not necessary if the receiving is hooked.
examples/receiving/recv_example_2.cc shows how the hooking works.
