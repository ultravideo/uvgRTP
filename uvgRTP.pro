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
	src/formats/h265.cc \
	src/formats/h266.cc \
	src/zrtp/zrtp_message.cc \
	src/zrtp/zrtp_receiver.cc \
	src/zrtp/hello.cc \
	src/zrtp/hello_ack.cc \
	src/zrtp/commit.cc \
	src/zrtp/dh_kxchng.cc \
	src/zrtp/confirm.cc \
	src/zrtp/confack.cc \
	src/zrtp/error.cc \
	src/srtp/base.cc \
	src/srtp/srtp.cc \
	src/srtp/srtcp.cc \

HEADERS += \
	include/clock.hh \
	include/crypto.hh \
	include/debug.hh \
	include/frame.hh \
	include/lib.hh \
	include/media_stream.hh \
	include/rtcp.hh \
	include/runner.hh \
	include/session.hh \
	include/socket.hh \
	include/util.hh \
	src/dispatch.hh \
	src/holepuncher.hh \
	src/hostname.hh \
	src/mingw_inet.hh \
	src/multicast.hh \
	src/pkt_dispatch.hh \
	src/poll.hh \
	src/queue.hh \
	src/random.hh \
	src/rtp.hh \
	src/zrtp.hh \
	src/formats/media.hh \
	src/formats/h26x.hh \
	src/formats/h264.hh \
	src/formats/h265.hh \
	src/zrtp/zrtp_receiver.hh \
	src/zrtp/zrtp_message.hh \
	src/zrtp/hello.hh \
	src/zrtp/hello_ack.hh \
	src/zrtp/commit.hh \
	src/zrtp/dh_kxchng.hh \
	src/zrtp/confirm.hh \
	src/zrtp/confack.hh \
	src/zrtp/error.hh \
	src/srtp/base.hh \
	src/srtp/srtp.hh \
	src/srtp/srtcp.hh \


unix {
    target.path = /usr/lib
    INSTALLS += target
}
