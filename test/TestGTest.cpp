#include "version.h"
#include <gtest/gtest.h>

TEST(DefaultTest, ctor) {
  EXPECT_STREQ("2.0.1", uvgrtp::get_version().c_str());
}