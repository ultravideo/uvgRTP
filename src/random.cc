#include "random.hh"

#include <limits>
#include <mutex>
#include <random>

/* std::mt19937 is not thread safe and this generator is shared by every stream (SSRCs, sequence
 * numbers, timestamps are generated from it, possibly from several threads at the same time) */
static std::mutex rng_mutex;
static std::mt19937 rng{std::random_device{}()};
static std::uniform_int_distribution<uint32_t> gen32_dist{
    1, std::numeric_limits<uint32_t>::max()};

uint32_t uvgrtp::random::generate_32() {
    std::lock_guard<std::mutex> lg(rng_mutex);
    return gen32_dist(rng);
}
