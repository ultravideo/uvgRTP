#pragma once

#include "uvgrtp/definitions.hh"
#include "uvgrtp/export.hh"

#include <cstdint>
#include <string>

namespace uvgrtp {

uint16_t get_version_major();
uint16_t get_version_minor();
uint16_t get_version_patch();

#if UVGRTP_EXTENDED_API
std::string get_version();
std::string get_git_hash();
#endif

} // namespace uvgrtp
