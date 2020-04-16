TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += sending.cc
INCLUDEPATH = $$PWD/../include
LIBS += -L$$PWD/../lib -lkvzrtp -lws2_32 -lpthread
