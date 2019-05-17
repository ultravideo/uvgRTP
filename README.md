# rtplib

Based on Marko Viitanen's [fRTPlib](https://github.com/fador/fRTPlib)

# building

```
mkdir build
cd build
cmake ..
make
sudo make install
```

# defines

if you want to enable run-time rtp statistics, use __RTP_STATS__

if you want to disable all prints (the rtp lib is quite verbose), use __RTP_SILENT__
