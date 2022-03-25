#include "uvgrtp/version.hh"

#include <cstdint>
#include <string>

namespace uvgrtp {
std::string get_version() {return std::string(uvgrtp_VERSION) + std::string("-qt");}

uint16_t get_version_major() { return uvgrtp_VERSION_MAJOR; }

uint16_t get_version_minor() { return uvgrtp_VERSION_MINOR; }

uint16_t get_version_patch() { return uvgrtp_VERSION_PATCH; }

std::string get_git_hash() {return "";}
} // namespace uvgrtp
