# kvzRTP example codes

We provide several simple and thoroughly commented examples on how to use kvzRTP.

[How to create a simple RTP sender](sending.cc)

[How to use fragmented input with kvzRTP \(HEVC slices\)](sending_fragmented.cc)

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

