.PHONY: all clean uvgrtp ffmpeg

CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 -g

uvgrtp_receiver:
	$(CXX) $(CXXFLAGS) -o uvgrtp/receiver uvgrtp/receiver.cc util/util.cc -luvgrtp -lkvazaar -lpthread

uvgrtp_sender:
	$(CXX) $(CXXFLAGS) -o uvgrtp/sender uvgrtp/sender.cc util/util.cc -luvgrtp -lkvazaar -lpthread

uvgrtp_latency:
	$(CXX) $(CXXFLAGS) -o uvgrtp/latency uvgrtp/latency.cc util/util.cc -luvgrtp -lkvazaar -lpthread

ffmpeg_sender:
	$(CXX) $(CXXFLAGS) -Wno-unused -Wno-deprecated-declarations -Wno-unused-result -o ffmpeg/sender \
		ffmpeg/sender.cc util/util.cc -lkvazaar `pkg-config --libs libavformat`

ffmpeg_receiver:
	$(CXX) $(CXXFLAGS) -Wno-unused -Wno-deprecated-declarations -Wno-unused-result -o ffmpeg/receiver \
		ffmpeg/receiver.cc util/util.cc -lkvazaar `pkg-config --libs libavformat`

ffmpeg_latency:
	$(CXX) $(CXXFLAGS) -Wno-unused -Wno-deprecated-declarations -Wno-unused-result -o ffmpeg/latency \
		ffmpeg/latency.cc util/util.cc -lkvazaar `pkg-config --libs libavformat` \

clean:
	rm -f uvgrtp/receiver uvgrtp/sender ffmpeg/sender ffmpeg/receiver ffmpeg/latency
