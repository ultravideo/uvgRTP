.PHONY: all clean uvgrtp ffmpeg gstreamer

CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 -g

uvgrtp_receiver:
	$(CXX) $(CXXFLAGS) -o uvgrtp/receiver uvgrtp/receiver.cc util/util.cc -luvgrtp -lkvazaar -lpthread -lcryptopp

uvgrtp_sender:
	$(CXX) $(CXXFLAGS) -o uvgrtp/sender uvgrtp/sender.cc util/util.cc -luvgrtp -lkvazaar -lpthread -lcryptopp

uvgrtp_latency:
	$(CXX) $(CXXFLAGS) -o uvgrtp/latency uvgrtp/latency.cc util/util.cc -luvgrtp -lkvazaar -lpthread

ffmpeg_sender:
	$(CXX) $(CXXFLAGS) -o ffmpeg/sender ffmpeg/sender.cc util/util.cc -lkvazaar `pkg-config --libs libavformat` \
		-Wno-unused -Wno-deprecated-declarations -Wno-unused-result

ffmpeg_receiver:
	$(CXX) $(CXXFLAGS) -o ffmpeg/receiver ffmpeg/receiver.cc util/util.cc -lkvazaar `pkg-config --libs libavformat` \
		-Wno-unused -Wno-deprecated-declarations -Wno-unused-result

ffmpeg_latency:
	$(CXX) $(CXXFLAGS) -o ffmpeg/latency ffmpeg/latency.cc util/util.cc -lkvazaar `pkg-config --libs libavformat` \
		-Wno-unused -Wno-deprecated-declarations -Wno-unused-result

clean:
	rm -f uvgrtp/receiver uvgrtp/sender ffmpeg/sender ffmpeg/receiver

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

# latency_ffmpeg:
# 	$(CXX) $(CXXFLAGS) -o main_latency_ffmpeg latency_ffmpeg.cc ../util/util.cc -lkvazaar `pkg-config --libs libavformat`

# clean:
# 	rm -f main_uvgrtp main_jrtp main_ccrtp main_live555 main_ortp main_ffmpeg main_latency_uvgrtp main_latency_ffmpeg main_gstreamer
