#-------------------------------------------------
#
# Project created by QtCreator 2019-06-03T09:27:47
#
#-------------------------------------------------

QT -= core gui

TARGET   = kvzrtp
TEMPLATE = lib
CONFIG  += staticlib

DEFINES += KVZRTP_LIBRARY

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

SOURCES += \
    src/send.cc \
    src/rtp_opus.cc \
    src/rtp_hevc.cc \
    src/rtp_generic.cc \
    src/reader.cc \
    src/lib.cc \
    src/frame.cc \
    src/conn.cc \
    src/writer.cc \
    src/mingw_inet.cc \
    src/multicast.cc \
    src/poll.cc \
    src/rtcp.cc \
    src/socket.cc \
    src/clock.cc \
    src/hostname.cc \
    src/queue.cc \
    src/random.cc

HEADERS += \
    src/writer.hh \
    src/send.hh \
    src/rtp_opus.hh \
    src/rtp_hevc.hh \
    src/rtp_generic.hh \
    src/rtcp.hh \
    src/reader.hh \
    src/lib.hh \
    src/frame.hh \
    src/conn.hh \
    src/debug.hh \
    src/util.hh \
    src/mingw_inet.hh \
    src/multicast.hh \
    src/poll.hh \
    src/rtcp.hh \
    src/socket.hh \
    src/clock.hh \
    src/hostname.hh \
    src/queue.hh \
    src/random.hh

unix {
    target.path = /usr/lib
    INSTALLS += target
}
