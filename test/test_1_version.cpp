#include "uvgrtp/lib.hh"
#include "uvgrtp/definitions.hh"

#include <gtest/gtest.h>


TEST(VersionTests, version) {
  
#if UVGRTP_EXTENDED_API
  EXPECT_STRNE("", uvgrtp::get_version().c_str());
#endif

  EXPECT_NE(0, uvgrtp::get_version_major());
}