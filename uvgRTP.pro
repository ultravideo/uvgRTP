#-------------------------------------------------
#
# Project created by QtCreator 2019-06-03T09:27:47
#
#-------------------------------------------------

QT -= core gui

TARGET   = uvgrtp
TEMPLATE = lib
CONFIG  += staticlib c++11

DEFINES += UVGRTP_LIBRARY

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_CXXFLAGS += -DNDEBUG
INCLUDEPATH    += include

SOURCES += \
	src/clock.cc \
	src/crypto.cc \
	src/dispatch.cc \
	src/frame.cc \
	src/hostname.cc \
	src/lib.cc \
	src/media_stream.cc \
	src/mingw_inet.cc \
	src/multicast.cc \
	src/pkt_dispatch.cc \
	src/poll.cc \
	src/queue.cc \
	src/random.cc \
	src/rtcp.cc \
	src/rtp.cc \
	src/runner.cc \
	src/session.cc \
	src/socket.cc \
	src/holepuncher.cc \
	src/zrtp.cc \
	src/formats/media.cc \
	src/formats/h26x.cc \
	src/formats/h264.cc \
	src/formats/h264_pkt_handler.cc \
	src/formats/h265.cc \
	src/formats/h265_pkt_handler.cc \
	src/formats/h266.cc \
	src/formats/h266_pkt_handler.cc \
	src/zrtp/zrtp_receiver.cc \
	src/zrtp/hello.cc \
	src/zrtp/hello_ack.cc \
	src/zrtp/commit.cc \
	src/zrtp/dh_kxchng.cc \
	src/zrtp/confirm.cc \
	src/zrtp/confack.cc \
	src/zrtp/error.cc \
	src/rtcp/app.cc \
	src/rtcp/sdes.cc \
	src/rtcp/bye.cc \
	src/rtcp/receiver.cc \
	src/rtcp/sender.cc \
	src/rtcp/rtcp_runner.cc \
	src/srtp/base.cc \
	src/srtp/srtp.cc \
	src/srtp/srtcp.cc \

HEADERS += \
	include/clock.hh \
	include/crypto.hh \
	include/debug.hh \
	include/dispatch.hh \
	include/frame.hh \
	include/hostname.hh \
	include/lib.hh \
	include/media_stream.hh \
	include/mingw_inet.hh \
	include/multicast.hh \
	include/pkt_dispatch.hh \
	include/holepuncher.hh \
	include/poll.hh \
	include/queue.hh \
	include/random.hh \
	include/rtcp.hh \
	include/rtp.hh \
	include/runner.hh \
	include/session.hh \
	include/socket.hh \
	include/util.hh \
	include/zrtp.hh \
	include/formats/media.hh \
	include/formats/h26x.hh \
	include/formats/h264.hh \
	include/formats/h265.hh \
	include/zrtp/zrtp_receiver.hh \
	include/zrtp/hello.hh \
	include/zrtp/hello_ack.hh \
	include/zrtp/commit.hh \
	include/zrtp/dh_kxchng.hh \
	include/zrtp/confirm.hh \
	include/zrtp/confack.hh \
	include/zrtp/error.hh \
	include/srtp/base.hh \
	include/srtp/srtp.hh \
	include/srtp/srtcp.hh \


unix {
    target.path = /usr/lib
    INSTALLS += target
}
