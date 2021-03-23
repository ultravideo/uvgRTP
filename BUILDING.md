# Building

uvgRTP is built using CMake and a 64-bit compiler is required for compilation.

uvgRTP can be built with QtCreator too, see uvgRTP.pro.

## Note about Crypto++ and SRTP/ZRTP support

uvgRTP uses [*__has_include*](https://en.cppreference.com/w/cpp/preprocessor/include) to detect if Crypto++ is present in the file system. Thus, SRTP/ZRTP is automatically enabled/disabled based whether it's found in the file system requiring no extra work from the user.

If, for some reason, you have Crypto++ available but would like to disable SRTP/ZRTP anyway, plase compile uvgRTP with `-DDISABLE_CRYPTO=1`, see the example below for more details.

## Examples

### Build uvgRTP

Building

```
mkdir build && cd build
cmake ..
make
sudo make install
```

Linking if Crypto++ was **not** found in the filesystem

```
g++ main.cc -luvgrtp -lpthread
```

Linking if Crypto++ was found in the filesystem

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
