# kvzRTP

kvzRTP is a thread-safe, easy-to-use RTP library written for next-generation video conferencing solutions. In ideal conditions it's able to achieve up to 607 MB/s goodput for HEVC stream.

There's no global state between the threads which results in no locking, it features a very easy-to-use and intuitive API and it's licensed under the permissive BSD 2-Clause License.

Based on Marko Viitanen's [fRTPlib](https://github.com/fador/fRTPlib)

## Usage

We provide several simple and thoroughly commented examples on how to use kvzRTP, please see:

[How to create a simple RTP sender](examples/simple/rtp/sending.cc)

[How to configure RTP sender to send high-quality stream](examples/simple/rtp/sending_hq.cc)

[How to create a simple RTP receiver (hooking)](examples/simple/rtp/receiving_hook.cc)

NOTE: The hook should **not** be used for media processing. It should be rather used as interface between application and library where the frame handout happens.

[How to create a simple RTP receiver (polling)](examples/simple/rtp/receiving_poll.cc)

[How to create an RTCP instance (polling)](examples/simple/rtcp/rtcp_poll.cc)

[How to create an RTCP instance (hoooking)](examples/simple/rtcp/rtcp_hook.cc)

### Memory ownership/deallocation

If you have not enabled the system call dispatcher, you don't need to worry about these

[Method 1, unique_ptr](examples/simple/rtp/deallocation_1.cc)

[Method 2, copying](examples/simple/rtp/deallocation_2.cc)

[Method 3, deallocation hook](examples/simple/rtp/deallocation_3.cc)

## Building

Linux
```
make -j8
sudo make install
```

You can also use QtCreator to build the library. The library must be built using a 64-bit compiler!

## Linking

The library should be linked as a static library to your program.

Linux

`-L<path to library folder> -lkvzrtp -lpthread`

Windows

`-L<path to library folder> -lkvzrtp -lpthread -lwsock32 -lws2_32`

## How is sending optimized?

For kvzRTP we've used 3 methods to improve the send throughput:
* Vectored I/O
   * This makes our stack zero copy. Both Linux and Windows network stacks allow scatter/gather type I/O where we can construct one complete UDP packet from smaller buffers
* System call batching (Linux only)
   * kvzRTP utilizes system call batching to reduce the overhead caused by system calls. It gathers all packets into a special data structure and flushes these packets all at once reducing the amount of system calls to one per _N_ packets. There's one-to-one correlation between number of `push_frame()`'s and number of system calls perfomed (regarless of the payload size supplied to `push_frame()`)
* System call dispatching
   * To improve the responsiveness of the library, application code only needs to call `push_frame()` which does the packetization and pushes the packets to a writer-specific frame queue. The actual sending is done by a separate thread called system call dispatcher (SCD). Its sole purpose is to take the packets from the frame queue and send them. This minimizes the delay experienced by the calling application and makes sending faster

System call batching and dispatching are a very tightly-knit combination. Frame queue is implemented as a vector of media transactions. For every `push_frame()`, a new transaction entry is created. Transaction entries contain pointers to media frame, RTP headers and the outgoing address.
This model makes the sending very clean because for every input there's one discrete output record which can be processed immediately or given to SCD. It is, however, not mandatory to enable SCD if frame queue want to be used. SCD brings benefits (smaller delay experienced by the application, better throughput) but the downside is that there's yet another thread running in the background. The goal is the dedicate one physical core for SCDs to minimize cache thrashing.

Because of these optimizations, we're the [fastest at sending HEVC](https://google.com) (and AVC [not tested]). Basically we're fast at sending any media stream with frame sizes big and small that require packetization to MTU-sized chunks.

Some of these optimizations, however, have some penalties and they must be enabled explicitly, see "Defines" for more details.

## Deallocation and memory ownership

Enabling SCD makes the lifetime of a transaction unknown. It might be processed immediately, it might take 2ms before it's processed. This is why no memory can be stored on the caller's stack unless the calling application knows that its stack frame has a longer lifetime than the transaction (kvzRTP doesn't provide any tools to figure this out so it's very risky). This also means that once `push_frame()` is called with some memory chunk, kvzRTP owns that memory (meaning it cannot be deallocated by the calling application).

Another problem is that kvzRTP doesn't know what **kind of** memory is given to it: does the pointer point to stack or heap, or perhaps to a memory-mapped file? This "opaqueness" of the pointer makes deallocation a dangerous task which is why kvzRTP needs help from the application.

There are few ways to deal with this memory ownership/deallocation issue:
* Provide deallocation callback function for the SCD:
   * When SCD has processed the transaction, it will call the deallocation callback function which deallocates the memory properly
* Use `unique_ptr`/`shared_ptr`
   * This is the advised way of using `push_frame()` because it's the easiest both from application's and kvzRTP's viewpoint
* Give `RTP_COPY` flag for `push_frame()`
   * This will cause kvzRTP to make a copy of the memory chunk and deallocate it when it's done. The calling application can also deallocate the memory it gave to kvzRTP without worries. This is also a very simple way of dealing with the issue but it degrades performance.
* Disable SCD
   * This is not advised, but it completely eliminates the ownership/deallocation problem

If SCD is used, it's **very important** to use one of the aforementioned tactics when calling `push_frame()`. Below is listed three possible error conditions that can happen if application doesn't follow these rules.

#### Problem 1
If application doesn't provide a deallocation callback, doesn't use `unique_ptr`/`shared_ptr` and doesn't pass `RTP_COPY` to `push_frame()`, kvzRTP assumes that the pointer points to stack and **won't do anything** about it. If the memory is on the heap, this will leak a lot of memory.

#### Problem 2
If deallocation callback is provided, kvzRTP assumes that application won't deallocate the memory by itself. If the application decides to deallocate the memory without kvzRTP's permission anyway, kvzRTP's deallocation call will results in a double-free.

#### Problem 3
If the memory becomes invalid while the transaction is waiting/being processed, SCD will sent garbage. Application must not assume anything about the lifetime of the memory given to `push_frame()` and rather use "persistent" memory like heap or somehow ensure that the lifetime of memory won't expire while the transaction is being processed.

By adhering to the memory ownership/deallocation scheme presented above, the application can be sure that there will be no memory issues stemming from kvzRTP.

## Optimistic fragment receiver (OFR)

kvzRTP features a special kind of fragment receiver that tries to minimize the number of copies. The number of copies is minimized by assuming that UDP packets come in order even though the protocol itself doesn't guarantee that (hence optimistic).

Though kvzRTP features an efficient receiver for HEVC that is able to reach the high goodput (~500 MB/s), it uses more CPU due to conservative assumptions about packet order. OFR reaches the same receive goodput but with 14% lower CPU usage and is thus recommended to be used for video conferencing situations with high bitrate and multiple participants.

## Defines

Use `__RTP_SILENT__` to disable all prints

Use `NDEBUG` to disable `LOG_DEBUG` which is the most verbose level of logging

# Adding support for new media types

Adding support for new media types quite straight-forward:
* add the payload to util.hh's `RTP_FORMAT` list
* create files to src/formats/`format_name`.{cc, hh}
* create `namespace format_name` inside `namespace kvz_rtp`
* Add functions `push_frame()` and `frame_receiver()`
   * You need to implement all (de)fragmentation required by the media type

See src/formats/hevc.cc and src/formats/hevc.hh for help when in doubt.

