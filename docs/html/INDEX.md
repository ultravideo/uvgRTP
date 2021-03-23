# uvgRTP public API documentation

To use uvgRTP, you must first create a uvgrtp::context object

Then you need to allocate a uvgrtp::session object from the context object by calling uvgrtp::context::create_session()

Finally, you need to allocate a uvgrtp::media_stream object from the allocated session object
by calling uvgrtp::session::create_stream()

This object is used for both sending and receiving, see documentation for
uvgrtp::media_stream::push_frame(),
uvgrtp::media_stream::pull_frame() and
uvgrtp::media_stream::install_receive_hook() for more details.
