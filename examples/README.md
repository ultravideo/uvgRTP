# uvgRTP example codes

This directory contains a collection of commented and functioning examples that demonstrate the usage of uvgRTP. Currently, the API is very C-like and would benefit greatly from using more C++ features. Help with this would be greatly appreciated.

## Tutorial

You can either include files individually from the include-folder or use lib.hh to include all necessary files with one line:

```
#include <uvgrtp/lib.hh>
```

### Step 1: Create context

When using uvgRTP, you must always first create the uvgrtp::context object:

```
uvgrtp::context ctx;
```
### Step 2: Create session

Next, you will use the uvgrtp::context object to create uvgrtp::session objects. The uvgrtp::session object contains all media streams you are sending/receiving to/from single IP address. Broadcast addresses should also work. There are two options for creating this: 1) specify one address, role of which can be determined with RCE_SEND_ONLY or RCE_RECEIVE_ONLY flag later:

```
uvgrtp::session *sess = ctx.create_session("10.10.10.2");
```
or 2) specify both remote and local addresses:

```
uvgrtp::session *sess = ctx.create_session("10.10.10.2", "10.10.10.3");
```
Mixing IPv4 and IPv6 addresses is not possible.

### Step 3: Create media_stream

To send/receive actual media, a uvgrtp::media_stream object has to be created. The first parameter is the local port from which the sending happens and the second port is the port where the data is sent to (note that these are in the reverse order compared to creating the session). The third parameter specifies the RTP payload format which will be used for the outgoing and incoming data. The last parameter holds the flags that can be used to modify the behavior of created uvgrtp::media_stream. The flags can be combined using bitwise OR-operation(|). These flags start with prefix `RCE_` and the explanations can be found in docs folder of repository. RTCP can be enabled with `RCE_RTCP`-flag.

```
uvgrtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RCE_NO_FLAGS);
```

One port version of this also exists, to be used with RCE_SEND_ONLY and RCE_RECEIVE_ONLY flags:
```
uvgrtp::media_stream *strm = sess->create_stream(8888, RTP_FORMAT_GENERIC, RCE_RECEIVE_ONLY);
```

### Step 3.1: Encryption (optional)

The encryption can be enabled by specifying `RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP` or `RCE_SRTP | RCE_SRTP_KMNGMNT_USER` in the flags parameter of create_stream. The `RCE_SRTP_KMNGMNT_USER` requires calling `add_srtp_ctx(key, salt)` for the created uvgrtp::media_stream. 

### Step 3.2: Configure media_stream (optional)

Some of the uvgrtp::media_stream functionality can be configured after the stream has been created:
```
strm->configure_ctx(RCC_MTU_SIZE, 2312);
```

The flags start with prefix `RCC_` and the rest of the flags can be found in the docs folder. Also, see the configuration example for more details.

### Step 4: Sending data

Sending can be done by simple calling push_frame()-function on created uvgrtp::media_stream:

```
strm->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
```
See the sending example for more details. uvgRTP does not take ownership of the memory unless the data is provided with std::unique_ptr.

### Step 5: Receiving data

There are two alternatives to receiving data. Using pull_frame()-function:
```
auto frame = strm->pull_frame();
```

or function callback based approach (I would recommend this to minimize latency):

```
strm->install_receive_hook(nullptr, rtp_receive_hook);
```

If you use classes, you can give a pointer to your class in the first parameter and call it in your callback function (an std::function API does not exist yet). In both versions of receiving, the user will be responsible for releasing the memory with the following function:
```
uvgrtp::frame::dealloc_frame(frame);
```

### Step 6: Cleanup

Cleanup can be done with following functions:
```
sess->destroy_stream(strm);
ctx.destroy_session(sess);
```

### Simple sending example (non-working)

```
#include <uvgrtp/lib.hh>

/* g++ main.cc -luvgrtp -lpthread && ./a.out */

int main(void)
{
    uvgrtp::context ctx;
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    uvgrtp::media_stream *strm = sess->create_stream(8888, 8888, RTP_FORMAT_GENERIC, RCE_NO_FLAGS);

    strm->configure_ctx(RCC_MTU_SIZE, 2312);

    char *message  = (char *)"Hello, world!";
    size_t msg_len = strlen(message) + 1;

    for (;;) {
        strm->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
        auto frame = strm->pull_frame();
        fprintf(stderr, "Message: '%s'\n", frame->payload);
        uvgrtp::frame::dealloc_frame(frame);
    }

    sess->destroy_stream(strm);
    ctx.destroy_session(sess);
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

Make sure you have checked the [build instructions](../BUILDING.md#linking-uvgrtp-and-crypto-to-an-application) if you want to build the encryption examples with Visual Studio.

[How to use SRTP with ZRTP](srtp_zrtp.cc)

[How to use multi-stream SRTP with ZRTP](zrtp_multistream.cc)

[How to use SRTP with user-managed keys](srtp_user.cc)
