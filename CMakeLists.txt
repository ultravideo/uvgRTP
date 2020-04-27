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
	src/poll.cc
	src/queue.cc
	src/random.cc
	src/receiver.cc
	src/rtcp.cc
	src/rtp.cc
	src/runner.cc
	src/send.cc
	src/sender.cc
	src/session.cc
	src/socket.cc
	src/srtp.cc
	src/zrtp.cc
	src/formats/generic.cc
	src/formats/hevc.cc
	src/formats/hevc_recv_normal.cc
	src/formats/opus.cc
	src/formats/hevc_recv_optimistic.cc
)

target_include_directories(uvgrtp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/include/formats
		${PROJECT_SOURCE_DIR}/include/mzrtp
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
