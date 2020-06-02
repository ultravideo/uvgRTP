.PHONY: all clean

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
		ffmpeg/sender.cc util/util.cc -lkvazaar -lavformat -lavcodec -lswscale -lz -lavutil -lpthread

ffmpeg_receiver:
	$(CXX) $(CXXFLAGS) -Wno-unused -Wno-deprecated-declarations -Wno-unused-result -o ffmpeg/receiver \
		ffmpeg/receiver.cc util/util.cc -lkvazaar -lavformat -lavcodec -lswscale -lz -lavutil -lpthread

ffmpeg_latency:
	$(CXX) $(CXXFLAGS) -Wno-unused -Wno-deprecated-declarations -Wno-unused-result -o ffmpeg/latency \
		ffmpeg/latency.cc util/util.cc -lkvazaar -lavformat -lavcodec -lswscale -lz -lavutil -lpthread

live555_sender:
	$(CXX) $(CXXFLAGS) live555/sender.cc live555/source.cc util/util.cc -o live555/sender \
		-I /usr/local/include/liveMedia \
		-I /usr/local/include/groupsock  \
		-I /usr/local/include/BasicUsageEnvironment \
		-I /usr/local/include/UsageEnvironment \
		-lkvazaar -lpthread -lliveMedia -lgroupsock -lBasicUsageEnvironment \
		-lUsageEnvironment -lcrypto -lssl

live555_receiver:
	$(CXX) $(CXXFLAGS) live555/receiver.cc live555/sink.cc util/util.cc -o live555/receiver \
		-I /usr/local/include/liveMedia \
		-I /usr/local/include/groupsock  \
		-I /usr/local/include/BasicUsageEnvironment \
		-I /usr/local/include/UsageEnvironment \
		-lkvazaar -lpthread -lliveMedia -lgroupsock -lBasicUsageEnvironment \
		-lUsageEnvironment -lcrypto -lssl

live555_latency:
	$(CXX) $(CXXFLAGS) live555/latency.cc live555/latsource.cc \
		live555/latsink.cc util/util.cc -o live555/latency \
		-I /usr/local/include/liveMedia \
		-I /usr/local/include/groupsock  \
		-I /usr/local/include/BasicUsageEnvironment \
		-I /usr/local/include/UsageEnvironment \
		-lkvazaar -lpthread -lliveMedia -lgroupsock -lBasicUsageEnvironment \
		-lUsageEnvironment -lcrypto -lssl

clean:
	rm -f uvgrtp/receiver uvgrtp/sender  uvgrtp/latency \
		ffmpeg/receiver ffmpeg/sender ffmpeg/latency \
		live555/receiver live555/sender live555/latency
