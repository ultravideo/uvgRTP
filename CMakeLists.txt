cmake_minimum_required(VERSION 3.5)

project(uvgrtp)

add_library(uvgrtp STATIC
	src/clock.cc
	src/crypto.cc
	src/dispatch.cc
	src/frame.cc
	src/hostname.cc
	src/lib.cc
	src/media_stream.cc
	src/mingw_inet.cc
	src/multicast.cc
	src/pkt_dispatch.cc
	src/poll.cc
	src/queue.cc
	src/random.cc
	src/rtcp.cc
	src/rtp.cc
	src/runner.cc
	src/session.cc
	src/socket.cc
	src/zrtp.cc
	src/formats/media.cc
	src/formats/h26x.cc
	src/formats/h264.cc
	src/formats/h264_pkt_handler.cc
	src/formats/h265.cc
	src/formats/h265_pkt_handler.cc
	src/zrtp/zrtp_receiver.cc
	src/zrtp/hello.cc
	src/zrtp/hello_ack.cc
	src/zrtp/commit.cc
	src/zrtp/dh_kxchng.cc
	src/zrtp/confirm.cc
	src/zrtp/confack.cc
	src/zrtp/error.cc
	src/rtcp/app.cc
	src/rtcp/sdes.cc
	src/rtcp/bye.cc
	src/rtcp/receiver.cc
	src/rtcp/sender.cc
	src/rtcp/rtcp_runner.cc
	src/srtp/base.cc
	src/srtp/srtp.cc
	src/srtp/srtcp.cc
)

target_include_directories(uvgrtp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/include/formats
		${PROJECT_SOURCE_DIR}/include/zrtp
		${PROJECT_SOURCE_DIR}/include/rtcp
		${PROJECT_SOURCE_DIR}/include/srtp
)

if (UNIX)
	install(TARGETS uvgrtp
			ARCHIVE
			DESTINATION /usr/local/lib
	)
	install(DIRECTORY src/ DESTINATION /usr/local/include/uvgrtp
			FILES_MATCHING PATTERN "*.hh"
	)
endif (UNIX)

if (WIN32)
	install(TARGETS uvgrtp
		    ARCHIVE
			DESTINATION ${PROJECT_BINARY_DIR}/lib
	)
	install(DIRECTORY src/ DESTINATION ${PROJECT_BINARY_DIR}/include
			FILES_MATCHING PATTERN "*.hh"
	)
endif (WIN32)
