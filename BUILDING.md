# Building

uvgRTP is built using CMake and a 64-bit compiler is required for compilation. By default, crypto is enabled.

uvgRTP can be built with QtCreator too, see uvgRTP.pro. Archive merging is not supported when QtCreator is used.

## Examples

### Build uvgRTP with crypto enabled

Building

```
mkdir build && cd build
cmake ..
make
sudo make install
```

Linking

```
g++ main.cc -luvgrtp -lpthread -lcryptopp
```

### Build uvgRTP with crypto disabled

Building

```
mkdir build && cd build
cmake -DDISABLE_CRYPTO=1 ..
make
sudo make install
```

Linking

```
g++ main.cc -luvgrtp -lpthread
```

### Build uvgRTP and include POSIX threads and Crypto++ into the final library

Building

```
mkdir build && cd build
cmake -DCRYPTOPP_PATH=/usr/local/lib/libcryptopp.a -DPTHREADS_PATH=/usr/lib/libpthreads.a ..
make
sudo make install
```

Linking

```
g++ main.cc -luvgrtp
```
