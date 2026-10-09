#ifndef LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_
#define LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_

#include <string>

#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/schema3/report_assembly.hpp"

namespace lumice::raypath {

std::string PathFeatureReportToJson(const PathFeatureReport& result, const char* lumice_version);

// The schema3 form: same product input, the assembled schema3 model side. The production
// entry once the flip lands (666.3 Step 3); until then it is reachable from the v3 tests
// only. `result.unsupported_multicrystal` takes the v3 early shape and ignores `assembled`.
std::string PathFeatureReportV3ToJson(const PathFeatureReport& result, const schema3::AssembledSchema3Report& assembled,
                                      const char* lumice_version);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_
