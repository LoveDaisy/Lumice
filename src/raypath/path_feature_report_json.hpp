#ifndef LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_
#define LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_

#include <string>

#include "raypath/path_feature_report.hpp"

namespace lumice::raypath {

std::string ProductPathReportToJson(const ProductPathReport& result, const char* lumice_version);

std::string PathFeatureReportToJson(const PathFeatureReport& result, const char* lumice_version);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PATH_FEATURE_REPORT_JSON_HPP_
