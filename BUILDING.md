# Building

uvgRTP RTP library uses CMake to generate build files and you must use 64-bit compiler to build uvgRTP. uvgRTP supports building with GCC, MinGW and Microsoft Visual Studio.

As an alternative to CMake, uvgRTP also includes a Qt project file (uvgRTP.pro) for building using QtCreator. This method is not described in this tutorial.

## Dependencies

uvgRTP uses [Crypto++](https://www.cryptopp.com/) for SRTP/ZRTP support. It is also possible to use uvgRTP without Crypto++. uvgRTP uses [*__has_include*](https://en.cppreference.com/w/cpp/preprocessor/include) to detect if Crypto++ is present in the file system. Thus, SRTP/ZRTP functionality is automatically disabled if crypto++ is not found in the system.

If for some reason you have Crypto++ available but would like to disable SRTP/ZRTP anyway, you may compile uvgRTP with `-DDISABLE_CRYPTO=1`. See the instructions below for more details.

## Building uvgRTP

Install [CMake](https://cmake.org) and make sure it is found in PATH. You can use Git Bash or other command terminals to the run the CMake commands on Windows.

### Build configuration generation with CMake

First, create a folder for build configuration:

```
mkdir build && cd build
```

Then generate build configuration with CMake using the following command:

```
cmake ..
```

Alternatively, if you want to disable Crypto++, use command:
```
cmake -DDISABLE_CRYPTO=1 ..
```

If you are using MinGW for your compilation, use the following command the generate the build configuration instead:

```
cmake -G"MinGW Makefiles" ..
```

### Visual Studio compilation

NOTE: There are problems with a specific version of Visual Studio 2017 compilation due to Creators Updates resulting in target platform x64 not being available. Please use another version of Visual Studio or some other way of compiling uvgRTP if you face this issue.

##### Building

After you have created the build files with CMake, open the solution and build the x64 version of uvgRTP. If you want to include Crypto++ and it is not found in PATH, you may add its headers to `uvgrtp -> Properties -> C/C++ -> General -> Additional Include Directories` to make sure it is included.

##### Linking to an application

Add the compiled uvgRTP library and the headers in the include folder of uvgRTP (and Crypto++ library if desired) to the Visual Studio project properties of the application. 

NOTE: Some application dependencies (such as Qt) may require a specific build configuration of Crypto++ to work. See more details [here](https://cryptopp.com/wiki/Visual_Studio).

### Linux and MinGW compilation

##### Building

After generating the build configuration, run the following commands to compile and install uvgRTP:
```
make
sudo make install
```

##### Linking

If you have compiled uvgRTP to use Crypto++, use the following command for linking:
```
g++ main.cc -luvgrtp -lpthread -lcryptopp
```

Or if you are not using Crypto++:
```
g++ main.cc -luvgrtp -lpthread
```
