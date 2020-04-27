#-------------------------------------------------
#
# Project created by QtCreator 2019-06-03T09:27:47
#
#-------------------------------------------------

QT -= core gui

TARGET   = uvgrtp
TEMPLATE = lib
CONFIG  += staticlib

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
    src/send.cc \
    src/lib.cc \
    src/frame.cc \
    src/mingw_inet.cc \
    src/multicast.cc \
    src/poll.cc \
    src/rtcp.cc \
    src/socket.cc \
    src/clock.cc \
    src/hostname.cc \
    src/queue.cc \
    src/random.cc \
    src/dispatch.cc \
    src/runner.cc \
    src/sender.cc \
    src/receiver.cc \
    src/session.cc \
    src/rtp.cc \
    src/srtp.cc \
    src/crypto.cc \
    src/media_stream.cc \
    src/formats/opus.cc \
    src/formats/hevc.cc \
    src/formats/hevc_recv_normal.cc \
    src/formats/generic.cc \
    # src/formats/hevc_recv_optimistic.cc \

HEADERS += \
    include/send.hh \
    include/rtcp.hh \
    include/lib.hh \
    include/frame.hh \
    include/debug.hh \
    include/util.hh \
    include/mingw_inet.hh \
    include/multicast.hh \
    include/poll.hh \
    include/rtcp.hh \
    include/socket.hh \
    include/clock.hh \
    include/hostname.hh \
    include/queue.hh \
    include/random.hh \
    include/dispatch.hh \
    include/runner.hh \
    include/receiver.hh \
    include/sender.hh \
    include/session.hh \
    include/rtp.hh \
    include/media_stream.hh \
    include/formats/opus.hh \
    include/formats/hevc.hh \
    include/formats/generic.hh

unix {
    target.path = /usr/lib
    INSTALLS += target
}
