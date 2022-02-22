#include "uvgrtp/lib.hh"
#include <gtest/gtest.h>

TEST(VersionTests, version) {
  EXPECT_STRNE("", uvgrtp::get_version().c_str());
}