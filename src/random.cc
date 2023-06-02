#include "random.hh"

#include <limits>
#include <random>

static std::mt19937 rng{std::random_device{}()};
static std::uniform_int_distribution<uint32_t> gen32_dist{
    1, std::numeric_limits<uint32_t>::max()};

uint32_t uvgrtp::random::generate_32() {
    return gen32_dist(rng);
}
