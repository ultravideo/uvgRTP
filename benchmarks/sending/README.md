# Send throughput benchmarks

The test setup is simple and tries to simulate a simple video conferencing condition where the RTP library is given a chunk of encoded video (H.265/HEVC in this case) and its job is to find the NAL units and send them (and possibly fragment the frame into smaller packets).

Sending the data as discrete NAL units and fragmenting large frames is required by the [RFC 7789](https://tools.ietf.org/html/rfc7798).

To eliminate all other performance-degrading factors and to get the maximum output of each library, the HEVC file was encoded beforehand and it was memory-mapped to the address space for faster reading.

The benchmark tests were on 64-bit Ubuntu Linux with 3.4 GHz Intel i7-4770 and the network card is Intel Ethernet Connection I217-LM (1GbE).

## Notes about the benchmark implementation

JRTPLIB, oRTP and ccRTP are not media-aware and won't do NAL unit extraction nor fragmentation so the test setup must provide this extra functionality.

Some of the libraries don't restrict the frame size at all which results in larger-than-MTU packets. To make comparison fair for all libraries, 1500 was chosen to be the maximum write size and no packet sent is larger than 1500 bytes.
