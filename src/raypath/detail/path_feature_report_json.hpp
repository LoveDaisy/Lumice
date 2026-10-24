#ifndef LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_
#define LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_

#include <string>

#include "raypath/feature_report_result.hpp"

namespace lumice::raypath {

// The v3 report serializer — the production entry behind `--report` and the C API's
// `LUMICE_PathFeatureReportToJson` (the v2 document assembly was removed in the same commit
// that flipped the public face to schema_version 3). `result.unsupported_multicrystal` takes
// the v3 early shape and ignores `assembled`.
std::string PathFeatureReportV3ToJson(const FeatureReportResult& result, const char* lumice_version);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_
