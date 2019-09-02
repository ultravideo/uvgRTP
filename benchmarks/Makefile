.PHONY: all clean kvzrtp jrtp ccrtp live555 ortp ffmpeg

CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 -g

all: kvzrtp jrtp ccrtp live555 ortp ffmpeg

kvzrtp:
	$(CXX) $(CXXFLAGS) -o main_kvzrtp kvzrtp.cc util/util.cc -lkvazaar -lkvzrtp -lpthread

jrtp:
	$(CXX) $(CXXFLAGS) -o main_jrtp jrtp.cc util/util.cc -lkvazaar -ljrtp -lpthread

ccrtp:
	$(CXX) $(CXXFLAGS) -o main_ccrtp ccrtp.cc util/util.cc -lkvazaar -lpthread -lccrtp -lcommoncpp -lucommon

live555:
	$(CXX) $(CXXFLAGS) -o main_live555 live555.cc util/util.cc util/live555_util.cc \
		-I /usr/local/include/liveMedia \
		-I /usr/local/include/groupsock  \
		-I /usr/local/include/BasicUsageEnvironment \
		-I /usr/local/include/UsageEnvironment \
		-lkvazaar -lpthread -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment

ortp:
	$(CXX) $(CXXFLAGS) -o main_ortp ortp.cc util/util.cc -lkvazaar -lpthread -lbctoolbox -lortp

ffmpeg:
	$(CXX) $(CXXFLAGS) -o main_ffmpeg ffmpeg.cc util/util.cc -lkvazaar -lpthread

clean:
	rm -f main_kvzrtp main_jrtp main_ccrtp main_live555 main_ortp main_ffmpeg
