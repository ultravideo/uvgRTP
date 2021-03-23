# Building

uvgRTP is built using [CMake](https://cmake.org) and a 64-bit compiler is required for compilation. uvgRTP can be built on both Linux and Windows.

Alternatively, uvgRTP can be built with QtCreator (see uvgRTP.pro).

NOTE: There has been some issues with a specific version of Visual Studio 2017 compilation on Windows due to Creators Updates as target platform x64 may not be available.
Please use some other way of compiling uvgRTP, such as QtCreator, is you face this issue.

## Dependencies

uvgRTP uses [Crypto++](https://www.cryptopp.com/) for SRTP/ZRTP support. It is possible to use uvgRTP without Crypto++.

uvgRTP uses [*__has_include*](https://en.cppreference.com/w/cpp/preprocessor/include) to detect if Crypto++ is present in the file system. Thus, SRTP/ZRTP is automatically enabled/disabled based whether it's found in the file system requiring no extra work from the user.

If, for some reason, you have Crypto++ available but would like to disable SRTP/ZRTP anyway, plase compile uvgRTP with `-DDISABLE_CRYPTO=1`, see the example below for more details.

## CMake Build Instructions

Install CMake, on Windows make sure it is found in PATH. On Windows you can use Git Bash or other console to the run commands.

### Generating build files

Create build folder

`mkdir build && cd build`

### Run CMake

`cmake ..`

Alternatively, if you want to disable Crypto

`cmake -DDISABLE_CRYPTO=1 ..`

### Building on Windows

Use Visual Studio.

### Linking uvgRTP on Windows

?

### Building on Linux

```
make
sudo make install
```

### Linking uvgRTP on Linux

With Crypto++

`g++ main.cc -luvgrtp -lpthread -lcryptopp`

Without Crypto++

`g++ main.cc -luvgrtp -lpthread`
