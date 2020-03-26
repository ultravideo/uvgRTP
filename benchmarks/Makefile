.PHONY: all clean kvzrtp live555 ffmpeg gstreamer

CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 -g

#all: kvzrtp live555 ffmpeg gstreamer

kvzrtp_receiver:
	$(CXX) $(CXXFLAGS) -o kvzrtp/receiver kvzrtp/receiver.cc util/util.cc -lkvzrtp -lkvazaar -lpthread -lcryptopp

kvzrtp_sender:
	$(CXX) $(CXXFLAGS) -o kvzrtp/sender kvzrtp/sender.cc util/util.cc -lkvzrtp -lkvazaar -lpthread -lcryptopp

kvzrtp_latency:
	$(CXX) $(CXXFLAGS) -o kvzrtp/latency kvzrtp/latency.cc util/util.cc -lkvzrtp -lkvazaar -lpthread

# jrtp:
# 	$(CXX) $(CXXFLAGS) -o main_jrtp jrtp.cc ../util/util.cc -lkvazaar -ljrtp -lpthread

# gstreamer:
# 	$(CXX) $(CXXFLAGS) -o main_gstreamer gstreamer.cc ../util/util.cc -lkvazaar -lpthread \
# 		`pkg-config --cflags --libs gstreamer-1.0`

# ccrtp:
# 	$(CXX) $(CXXFLAGS) -o main_ccrtp ccrtp.cc ../util/util.cc -lkvazaar -lpthread -lccrtp -lcommoncpp -lucommon

# live555:
# 	$(CXX) $(CXXFLAGS) -o main_live555 live555.cc ../util/util.cc ../util/live555_util.cc \
# 		-I /usr/local/include/liveMedia \
# 		-I /usr/local/include/groupsock  \
# 		-I /usr/local/include/BasicUsageEnvironment \
# 		-I /usr/local/include/UsageEnvironment \
# 		-lkvazaar -lpthread -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment

# ortp:
# 	$(CXX) $(CXXFLAGS) -o main_ortp ortp.cc ../util/util.cc -lkvazaar -lpthread -lortp

# ffmpeg:
# 	$(CXX) $(CXXFLAGS) -o main_ffmpeg ffmpeg.cc ../util/util.cc -lkvazaar `pkg-config --libs libavformat`

# latency_ffmpeg:
# 	$(CXX) $(CXXFLAGS) -o main_latency_ffmpeg latency_ffmpeg.cc ../util/util.cc -lkvazaar `pkg-config --libs libavformat`

# clean:
# 	rm -f main_kvzrtp main_jrtp main_ccrtp main_live555 main_ortp main_ffmpeg main_latency_kvzrtp main_latency_ffmpeg main_gstreamer
