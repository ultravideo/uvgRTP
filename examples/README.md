# uvgRTP example codes

## Instructions for Windows

1) Run prepare.bat in this directory to prepare testing environment for uvgRTP
   * The script creates lib/ and include/ directories to project root directory
2) Build uvgRTP using QtCreator
3) When uvgRTP has been built, copy the libuvgrtp.a from the Qt build directory to lib/ created by the script
4) Open the example.pro and build & run it
   * Make sure the Qt build directory for example.pro is located in the same directory as lib/ and include/ (projects root directory) or tweak the include/lib paths accordingly

## Instructions for Linux

```
sudo make --directory=.. all install
g++ sending.cc -luvgrtp -lpthread && ./a.out
```

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

### Memory ownership/deallocation

If you have not enabled the system call dispatcher, you don't need to worry about these

[Method 1, unique_ptr](deallocation_1.cc)

[Method 2, copying](deallocation_2.cc)

[Method 3, deallocation hook](deallocation_3.cc)
