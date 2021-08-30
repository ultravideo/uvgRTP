#pragma once

#include <cstdint>
#include <string>

namespace uvgrtp {
std::string get_uvgrtp_version();
uint16_t get_uvgrtp_version_major();
uint16_t get_uvgrtp_version_minor();
uint16_t get_uvgrtp_version_patch();
std::string get_git_hash();
} // namespace uvgrtp
