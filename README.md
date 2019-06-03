# rtplib

Based on Marko Viitanen's [fRTPlib](https://github.com/fador/fRTPlib)

# building

Linux
```
make -j8
sudo make install
```

you can also use QtCreator to build the library

# defines

if you want to enable run-time rtp statistics, use `__RTP_STATS__`

if you want to disable all prints (the rtp lib is quite verbose), use `__RTP_SILENT__`

# api

## sending data

writer->pushFrame()

## receiving data

