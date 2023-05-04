# Building

uvgRTP RTP library uses CMake to generate build files and you must use 64-bit compiler to build uvgRTP. uvgRTP supports building with GCC, MinGW and Microsoft Visual Studio.

## Dependencies

uvgRTP has one optional dependency in [Crypto++](https://www.cryptopp.com/).

uvgRTP uses Crypto++ for the SRTP/ZRTP support. With compilers that support C++17, uvgRTP uses [*__has_include*](https://en.cppreference.com/w/cpp/preprocessor/include) to detect if Crypto++ is present in the file system. Thus, SRTP/ZRTP functionality is automatically disabled if crypto++ is not found in the system. If you use compiler that doesn't support __has_include, or if you have Crypto++ available but would like to disable SRTP/ZRTP anyway, you may compile uvgRTP with `-DDISABLE_CRYPTO=1`. See the instructions below for more details.

## Building uvgRTP

Install [CMake](https://cmake.org) and make sure it is found in PATH. On Windows, you can use Git Bash or other command terminals to the run the CMake commands.

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
cmake -DUVGRTP_DISABLE_CRYPTO=1 ..
```

If you are using MinGW for your compilation, add the generate parameter the generate the MinGW build configuration:

```
cmake -G"MinGW Makefiles" ..
```

### Visual Studio compilation

NOTE: There are problems with a specific version of Visual Studio 2017 compilation due to Creators Updates resulting in target platform x64 not being available. Please use another version of Visual Studio or some other way of compiling uvgRTP if you face this issue.

##### Building

After you have created the build files with CMake, open the solution and build the x64 version of uvgRTP. If you want to include Crypto++ and it is not found in PATH, you may add its headers to `uvgrtp -> Properties -> C/C++ -> General -> Additional Include Directories` to make sure it is included.

##### Linking to an application

Add the path to uvgRTP include folder to `project -> Properties -> C/C++ -> General -> Additional Include Directories` for your project. Add `uvgrtp.lib` to `project -> Properties -> Linker -> Input -> Additional Dependencies` as a dependancy and specify the location of built library by adding the path to `project -> Properties -> Linker -> General -> Additional Library Directories`. Another option for finding the headers and the library is adding them to PATH.

##### Linking uvgRTP and Crypto++ to an application

If Crypto++ is not found on Windows, it is assumed to be missing in uvgRTP to avoid unexpected build errors. You need to add 1) Crypto++ header location when compiling uvgRTP, 2) Crypto++ library dependancy for your project and 3) Crypto++ library location for you project. You can skip 1) and 3) if the files can be found in PATH. These instructions also apply to encryption portions of uvgRTP examples and test suite. 
1) Make sure that the uvgRTP library project can find Crypto++ headers by adding the parent directory of Crypto++ to `uvgrtp -> Properties -> C/C++ -> General -> Additional Include Directories` and making sure the folder is named `cryptopp`, otherwise Crypto++ support will be disabled. uvgRTP expects Crypto++ includes in format `cryptopp/<header name>.hh`.

2) Make sure that the application is linking `cryptlib.lib` by adding it to `project -> Properties -> Linker -> Input -> Additional Dependencies`.

3) Make sure that the application can find the built Crypto++ library by setting the library directory `project -> Properties -> Linker -> General -> Additional Library Directories`.

Note: Some application dependencies (such as Qt and newer Visual Studio versions) may require a specific runtime library for Crypto++ linking to work. If you get errors referring to static and dynamic runtime library version mismatch, please set 
```
<RuntimeLibrary>MultiThreaded</RuntimeLibrary> -> <RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>
```
and 
```
<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary> -> <RuntimeLibrary>MultiThreadeddDebugDLL</RuntimeLibrary>
```
in Crypto++ project file `cryptlib.vcxproj` and rebuild `cryptlib`. See more details [here](https://cryptopp.com/wiki/Visual_Studio).

### Linux and MinGW compilation

##### Selecting build type with CMake

If you are using a system that does not provide build type selection later, you may need to select Debug build with:

```
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

The default build type is Release.

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

You can also use `pkg-config` to get the flags.

## Silence all prints

It is possible to silence all prints coming from uvgRTP by enabling following parameter:

```
cmake -DUVGRTP_DISABLE_PRINTS=1 ..
```

## Disallow compiler warnings by enabling Werror flag

`-Werror`-flag is disabled by default, but you can enable it by disabling the following flag:

```
cmake -DUVGRTP_DISABLE_WERROR=0 ..
```
This is recommended before making a pull request.

## Not building examples or tests

By default, uvgRTP configures both examples and tests as additional targets to build. If this is undesirable, you can disable their configuration with following CMake parameters:

```
cmake -DUVGRTP_DISABLE_TESTS=1 -DUVGRTP_DISABLE_EXAMPLES=1 ..
```

## Release commit (for devs)

The release commit can be specified in CMake. This slightly changes how the version is printed. This feature is mostly useful for distributing release versions. Use the following command:

```
cmake -DUVGRTP_RELEASE_COMMIT=1 ..
```

