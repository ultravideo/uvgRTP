# Testing uvgRTP

Testing uvgRTP is meant for the developers of uvgRTP. After each major change, it is recommended to run the automated tests. New automated tests can be a great help in validating the functionality of new features as well as helping detect bugs caused by later changes.

These tests are not meant to be readable examples. For that you might want to check out [examples](../examples/).

## Building the automated tests

uvgRTP uses GoogleTest framework for automated testing. The automated tests are run separately after the build process. First, [build](../BUILDING.md) uvgRTP normally. Running the CMake will create a new folder called ```test``` under the ```build``` folder.

### GCC (Linux)

Install Crypto++. After this, run ```make``` in ```build/test``` folder. This will create a program called ```uvgrtp_test```. Run this program to run the automated tests.

### MSVC (Windows)

Open the generated solution. Add the location of compiled Crypto++ library to ```uvgrtp_test -> Properties -> Linker -> General -> Additional Library Directories``` unless it is found in PATH. Building the uvgrtp_test will generate the program in Debug/Release folder. Run the ```uvgrtp_test.exe``` to start the automated tests. Using a command line is recommended so the results don't disappear after finishing.

## Current test suites

Currently, the automated tests consist of the following test suites:
- [Version tests](test_1_version.cpp)
- [RTP tests](test_2_rtp.cpp)
- [RTCP tests](test_3_rtcp.cpp)
- [Format tests](test_4_formats.cpp)
- [SRTP + ZRTP tests](test_5_srtp_zrtp.cpp)


The tests should be coded in such a way to make the tests themselves as resilient as possible to problems while also validating that the uvgRTP output is correct. In other words, it is more helpful if a check is false than if the test suite crashes.

## Creating new tests

New tests can be created by adding a new function in one of the existing files or creating a new file. Each file should only contain tests from a single test suite and all tests of one test suite should be located in a single file. New files have to also be added to the local [CMakeLists.txt](CMakeLists.txt) file.

You can read about GoogleTest framework [here](https://google.github.io/googletest/).
