#pragma once


static inline void hex_dump(uint8_t* buf, size_t len)
{
    if (!buf)
        return;

    for (size_t i = 0; i < len; i += 10) {
        fprintf(stderr, "\t");
        for (size_t k = i; k < i + 10; ++k) {
            fprintf(stderr, "0x%02x ", buf[k]);
        }
        fprintf(stderr, "\n");
    }
}

static inline void set_bytes(int* ptr, int nbytes)
{
    if (ptr)
        * ptr = nbytes;
}

static inline void* memdup(const void* src, size_t len)
{
    uint8_t* dst = new uint8_t[len];
    std::memcpy(dst, src, len);

    return dst;
}