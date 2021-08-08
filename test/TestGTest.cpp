#include "uvgrtp_version.h"
#include <gtest/gtest.h>

TEST(DefaultTest, ctor) {
  EXPECT_STREQ("2.1.0", uvgrtp::get_uvgrtp_version().c_str());
}