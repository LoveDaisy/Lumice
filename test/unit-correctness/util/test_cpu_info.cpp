// Tests for src/util/cpu_info.{hpp,cpp} — PhysicalCoreCount() / LogicalCoreCount() invariants.

#include <thread>

#include "gtest/gtest.h"
#include "util/cpu_info.hpp"

namespace {

TEST(CpuInfoTest, PhysicalCoreCountIsAtLeastOne) {
  EXPECT_GE(lumice::PhysicalCoreCount(), 1);
}

TEST(CpuInfoTest, PhysicalCoreCountDoesNotExceedHardwareConcurrency) {
  auto hw = static_cast<int>(std::thread::hardware_concurrency());
  if (hw > 0) {
    EXPECT_LE(lumice::PhysicalCoreCount(), hw);
  } else {
    GTEST_SKIP() << "hardware_concurrency() returned 0; cannot bound PhysicalCoreCount()";
  }
}

TEST(CpuInfoTest, PhysicalCoreCountIsStable) {
  int a = lumice::PhysicalCoreCount();
  int b = lumice::PhysicalCoreCount();
  EXPECT_EQ(a, b);
}

TEST(CpuInfoTest, LogicalCoreCountIsAtLeastOne) {
  EXPECT_GE(lumice::LogicalCoreCount(), 1);
}

TEST(CpuInfoTest, LogicalCoreCountMatchesHardwareConcurrencyWhenKnown) {
  auto hw = static_cast<int>(std::thread::hardware_concurrency());
  if (hw > 0) {
    EXPECT_EQ(lumice::LogicalCoreCount(), hw);
  } else {
    GTEST_SKIP() << "hardware_concurrency() returned 0; LogicalCoreCount() can only be bounded below";
  }
}

// The cross-invariant the Windows automatic worker count rests on (server.cpp sizes it from
// LogicalCoreCount() there, from PhysicalCoreCount() elsewhere): a logical count never
// undercounts the physical one, on any platform.
TEST(CpuInfoTest, LogicalCoreCountIsAtLeastPhysicalCoreCount) {
  EXPECT_GE(lumice::LogicalCoreCount(), lumice::PhysicalCoreCount());
}

}  // namespace
