# Building

uvgRTP RTP library uses CMake to generate build files and you must use 64-bit compiler to build uvgRTP. uvgRTP supports building on both Linux and Windows.

Alternatively, uvgRTP can also be built with QtCreator for Qt applications (see uvgRTP.pro).

NOTE: There are with a specific version of Visual Studio 2017 compilation due to Creators Updates resulting in target platform x64 not being available. Please use another version of Visual Studio or some other way of compiling uvgRTP if you face this issue.

## Dependencies

uvgRTP uses [Crypto++](https://www.cryptopp.com/) for SRTP/ZRTP support. It is also possible to use uvgRTP without Crypto++. uvgRTP uses [*__has_include*](https://en.cppreference.com/w/cpp/preprocessor/include) to detect if Crypto++ is present in the file system. Thus, SRTP/ZRTP is automatically enabled/disabled based whether it's found in the file system.

If for some reason you have Crypto++ available but would like to disable SRTP/ZRTP anyway, please compile uvgRTP with `-DDISABLE_CRYPTO=1`. See the instructions below for more details.

## CMake Build Instructions

Install [CMake](https://cmake.org) and on Windows make sure it is found in PATH. You can use Git Bash or other command terminals to the run the CMake commands on Windows.

### Generating build files

First, create a folder for build scripts:

```
mkdir build && cd build
```

### Run CMake

Then run CMake with command:

```
cmake ..
```

Alternatively, if you want to disable Crypto++, use command:
```
cmake -DDISABLE_CRYPTO=1 ..
```

### Visual Studio

##### Building

After you have created the build files with CMake, open the solution and build the x64 version of uvgRTP.

##### Linking to an application

Add the compiled uvgRTP + headers (and Crypto++ if desired) to the Visual Studio project Configuration Properties of the application.

### Linux


##### Building

Run the following commands:
```
make
sudo make install
```

##### Linking

If you have compiled uvgRTP to use Crypto++, use the following command:
```
g++ main.cc -luvgrtp -lpthread -lcryptopp
```

Or if you are not using Crypto++:
```
g++ main.cc -luvgrtp -lpthread
```
