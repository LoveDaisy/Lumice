#pragma once

#include <string>

namespace lumice::gui {

// Test-only entry point into config_summary.cpp, kept in its own header on purpose — the same
// shape as theme_test_hooks.hpp, and for the same reason: no production translation unit has any
// reason to include a file named config_summary_test_hooks.hpp, so an accidental production call
// site shows up as an odd include line rather than as one more function in the page builder's
// public list. The only includer today is test/gui/test_gui_main.cpp.
//
// What it does: pins what BuildConfigSummary() writes into ConfigSummary::version, the one line
// of the Summary page whose content changes on every `scripts/version.py set`. The page prints
// the real LUMICE_GetVersionString() in the product and must keep doing so — a shared screenshot
// answers "which version rendered this" first. But the config_summary_layout reference images
// capture that line's pixels, so without this override a version bump turns that group red for a
// reason that has nothing to do with the layout under test.
//
// Note that config_summary.cpp defines this function without including this header, which is what
// keeps "no production TU has a reason to include it" true of the definer as well. The signature
// therefore has to match by hand: the compiler checks nothing here, and a mismatch that is
// link-compatible would diverge silently.
void SetConfigSummaryVersionForTest(std::string version);

}  // namespace lumice::gui
