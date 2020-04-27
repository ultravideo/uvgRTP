# Building

There are several ways to build uvgRTP: GNU make, CMake, QtCreator or Visual Studio

NB: uvgRTP must be built with a 64-bit compiler!

## Dependencies

The only dependency vanilla uvgRTP has is pthreads

## Visual Studio

Open uvgRTP.sln in Visual Studio and build the library

## Qt Creator

Open uvgrtp.pro in Qt Creator and build the library

## CMake + Ninja

```
mkdir build && cd build
cmake -GNinja ..
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

# Defines

Use `__RTP_SILENT__` to disable all prints

Use `__RTP_CRYPTO__` to enable SRTP/ZRTP and crypto routines

Use `NDEBUG` to disable `LOG_DEBUG` which is the most verbose level of logging
