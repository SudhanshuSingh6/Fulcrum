#include "core/version.h"

#include <gtest/gtest.h>

#include "version_config.h"

TEST(Version, MatchesCMakeProjectVersion) {
  EXPECT_FALSE(fulcrum::version().empty());
  EXPECT_EQ(fulcrum::version(), FULCRUM_VERSION_STRING);
}
