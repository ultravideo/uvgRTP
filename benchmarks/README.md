# Send throughput benchmarks

The test setup is simple and tries to simulate a simple video conferencing condition where the RTP library is given a chunk of encoded video (H.265/HEVC in this case).

To eliminate all other performance degrading factor and to get the maximum output of each library, the HEVC file was been encoded beforehand, it was memory-mapped and the sending was on localhost to minimize the netowork latency.

The task for each library is to find NAL units from the file and send them to remote (just a netcat server in listening mode). The actual video chunks (not VPS/SPS/PPS) are around 177 kB of memory so they must be splitted into smaller units. The test assumes MTU to be 1500 bytes.

The benchmark tests were on 64-bit Ubuntu Linux with 3.4 GHz Intel i7-4770. TODO network card info?

## Notes about the benchmark implementation

Two problems arose when designing the benchmark: JRTPLIB, ccRTP and Live555 are not media-aware in the same sense as kvzRTP is and they don't do automatic frame fragmentation like kvzRTP does.

This means that they either reject large packets (JRTPLIB) or send them as is (ccRTP, Live555) without respecting MTU. So the test code must do the fragmentation for these libraries.

They are also not media-aware and won't look for NAL units from the memory they're given and just send the data. The test code must also add code for finding the NAL units. TODO uudelleenkirjoita
