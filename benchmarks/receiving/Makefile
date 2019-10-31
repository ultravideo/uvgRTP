.PHONY: all clean kvzrtp # jrtp ccrtp live555 ortp ffmpeg

CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 -g -DBUILD_WITH_EASY_PROFILER

all: kvzrtp live555 ffmpeg

kvzrtp:
	$(CXX) $(CXXFLAGS) -o main_kvzrtp kvzrtp.cc ../util/util.cc -lkvzrtp -lkvazaar -lpthread -leasy_profiler

live555:
	$(CXX) $(CXXFLAGS) -o main_live555 live555.cc ../util/util.cc ../util/live555_util.cc \
		-I /usr/local/include/liveMedia \
		-I /usr/local/include/groupsock  \
		-I /usr/local/include/BasicUsageEnvironment \
		-I /usr/local/include/UsageEnvironment \
		-lkvazaar -lpthread -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment

ffmpeg:
	$(CXX) $(CXXFLAGS) -o main_ffmpeg ffmpeg.cc ../util/util.cc -lkvazaar `pkg-config --libs libavformat` \
		-Wno-deprecated-declarations -Wno-unused-variable -Wno-unused-but-set-variable \
		-Wno-unused-result -Wno-conversion-null -Wno-unused-parameter
clean:
	rm -f main_kvzrtp main_jrtp main_ccrtp main_live555 main_ortp main_ffmpeg
