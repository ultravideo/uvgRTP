# Sending

## HEVC sender

Extract 8-bit yuv420 raw video from input.mp4 and start ffplay

```
ffmpeg -i input.mp4 -f rawvideo -pix_fmt yuv420p video.raw
ffplay -protocol_whitelist "file,rtp,udp" ../sdp/hevc.sdp
```

Compile the RTP Library and hevc_sender.cc and start the sender

```
cd ../..
make all -j8
cd examples/sending
g++ -o main hevc_sender.cc -lrtp -L ../.. -lpthread -lopus
./main
```

## Opus sender

Extract signed 16-bit little endian PCM from input.mp4 and start ffplay

```
ffmpeg -i input.mp4 -f s16le -acodec pcm_s16le -ac 2 -ar 48000 output.raw
ffplay -acodec libopus -protocol_whitelist "file,rtp,udp" ../sdp/opus.sdp
```

Compile the RTP Library and opus_sender.cc and start the sender

```
cd ../..
make all -j8
cd examples/sending
g++ -o main opus_sender.cc -lrtp -L ../.. -lpthread -lopus
./main
```
