#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <string>

#include "include/lumice.h"

// LUMICE_GetEngineIsaLevel() is the engine's own run-time answer to "which ISA tier was I
// compiled for", the value the CLI's [BENCHMARK] `isa` key reports. This test pins the two
// facts a caller can rely on without knowing the configure:
//
//  1. The value is one of the four tier names — never NULL, never anything else.
//  2. It is either the tier this build was configured with (LUMICE_TEST_CONFIGURED_ISA_LEVEL,
//     handed to this binary by test/CMakeLists.txt from the same LUMICE_ISA_LEVEL the engine
//     was configured with) or "baseline" — the documented fallback for a non-Release config
//     and for real MSVC cl.exe, which has no -march equivalent. Anything else would mean the
//     engine reports a tier it was never asked to build for.
//
// What this test deliberately does NOT check is that the answer equals the configured tier
// exactly: whether the -march flag was applied depends on the config and the compiler, and
// the e2e layer (test/e2e-correctness/test_cli.py) already cross-checks the reported value
// against CMakeCache.txt with both of those in hand.
TEST(CApiEngineIsaLevel, AnswerIsATierName) {
  const char* isa = LUMICE_GetEngineIsaLevel();
  ASSERT_NE(isa, nullptr);
  const std::set<std::string> tiers = { "baseline", "x86-64-v3", "x86-64-v4", "native" };
  EXPECT_TRUE(tiers.count(isa) == 1) << "LUMICE_GetEngineIsaLevel() returned " << isa;
}

TEST(CApiEngineIsaLevel, AnswerIsTheConfiguredTierOrBaseline) {
  const std::string isa = LUMICE_GetEngineIsaLevel();
  EXPECT_TRUE(isa == "baseline" || isa == LUMICE_TEST_CONFIGURED_ISA_LEVEL)
      << "engine reports " << isa << " but this build was configured with " << LUMICE_TEST_CONFIGURED_ISA_LEVEL;
}

TEST(CApiEngineIsaLevel, AnswerIsStableAcrossCalls) {
  // Static storage: the same pointer every time, so a caller may keep it for the process.
  EXPECT_EQ(LUMICE_GetEngineIsaLevel(), LUMICE_GetEngineIsaLevel());
}
