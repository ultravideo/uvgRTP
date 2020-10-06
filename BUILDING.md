# Building

There are several ways to build uvgRTP: GNU make, CMake, or QtCreator

NB: uvgRTP must be built with a 64-bit compiler!

## Dependencies

The only dependency vanilla uvgRTP has is pthreads

## Qt Creator

Open uvgrtp.pro in Qt Creator and build the library

## CMake

```
mkdir build && cd build
cmake ..
ninja
```

## GNU make

```
make -j5
sudo make install
```

# Linking

Building uvgRTP produces a static library and it should be linked to the application as such:

```
-luvgrtp -lpthread
```

## Linking when SRTP/ZRTP is used

```
-luvgrtp -lcryptopp -lpthread
```

# Defines

Use `__RTP_SILENT__` to disable all prints

Use `__RTP_CRYPTO__` to enable SRTP/ZRTP and crypto routines

Use `NDEBUG` to disable `LOG_DEBUG` which is the most verbose level of logging
