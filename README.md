# rtplib

Based on Marko Viitanen's [fRTPlib](https://github.com/fador/fRTPlib)

# building

Linux
```
make -j8
sudo make install
```

you can also use QtCreator to build the library

# defines

if you want to disable all prints (the rtp lib is quite verbose), use `__RTP_SILENT__`

# API

## Sending data

Sending data is a simple as calling writer->push_frame(), see examples/sending/hevc_sender.cc.

## Receiving data

Reading frames can be done using two different ways: polling frames or installing a receive hook.

Polling frames is a blocking operation and a separate thread should be created for it.
examples/receiving/recv_example_1.cc shows how the polling approach works.

The second way to receive frames is to install a receive hook and when an RTP frame is received, this receive hook is called. Creating separate thread for reading data is not necessary if the receiving is hooked.
examples/receiving/recv_example_2.cc shows how the hooking works.
