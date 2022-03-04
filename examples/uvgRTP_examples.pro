TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

# define here which example you want to build. Remember to rerun qmake
CONFIG += selectBinding

selectBinding {
  DEFINES += __RTP_NO_CRYPTO__
  message("Building binding example.")
  TARGET = Binding
  SOURCES += \
    binding.cc \
}

selectConfiguration {
  message("Building configuration example.")
  TARGET = Configuration
  SOURCES += \
    configuration.cc \
}

selectCustomTimestamps {
  message("Building timestamp example.")
  TARGET = Timestamp
  SOURCES += \
    custom_timestamps.cc \
}

selectReceiveHook {
  message("Building receive hook example.")
  TARGET = ReceiveHook
  SOURCES += \
    receiving_hook.cc \
}

selectReceivePoll {
  message("Building receive poll example.")
  TARGET = ReceivePoll
  SOURCES += \
    receiving_poll.cc \
}

selectRTCPhook {
  message("Building RTCP hook example.")
  TARGET = RTCPHook
  SOURCES += \
    rtcp_hook.cc \
}

selectSending {
  message("Building sending example.")
  TARGET = Sending
  SOURCES += \
    sending.cc \
}

selectSendingGeneric {
  message("Building generic sending example.")
  TARGET = SendingGeneric
  SOURCES += \
    sending_generic.cc \
}

selectSRTPUser {
  message("Building user managed SRTP example.")
  TARGET = SRTPUser
  SOURCES += \
    srtp_user.cc \

}

selectZRTPMultistream {
  message("Building ZRTP multistream example.")
  TARGET = ZRTPMultistream
  SOURCES += \
    zrtp_multistream.cc \
}

DISTFILES += \
  README.md


# uvgrtp include folder
INCLUDEPATH    += ../include

LIBS += -luvgrtp

# uvgRTP links agains Crypto++ if it is available and excluded.
# You can safely remove its linking if you did not include Crypto++ in uvgRTP.
# Please note that the SRTP and ZRTP examples need Crypto++ to succeed
win32-msvc{
  LIBS += -lcryptlib
} else {
  LIBS += -lcryptopp
}

win32-msvc{
  message("Detected MSVC compiler")
  # find the libraries if uvgrtp has been built using CMake instructions
  CONFIG(debug, debug|release) {
    UVGRTP_LIB_FOLDER = -L$$PWD/../build/Debug
  } else:CONFIG(release, debug|release) {
    UVGRTP_LIB_FOLDER = -L$$PWD/../build/Release
  }

  LIBS += -lws2_32
  LIBS += -ladvapi32

  LIBS += $${UVGRTP_LIB_FOLDER}
  message("Using the following folder for uvgRTP library:" $${UVGRTP_LIB_FOLDER})
}
