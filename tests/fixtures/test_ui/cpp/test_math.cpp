#include <gtest/gtest.h>
TEST(Arithmetic, AdditionKeepsALongReadableTestName) { EXPECT_EQ(2 + 2, 4); }
TEST(Arithmetic, Failure) {
  std::cout << "C++ response payload" << std::endl;
  EXPECT_EQ(2 + 2, 5);
}
TEST(Arithmetic, Skipped) { GTEST_SKIP() << "demonstrate skipped result"; }
