#ifndef LUMICE_RAYPATH_SINGLE_PATH_JSON_HPP_
#define LUMICE_RAYPATH_SINGLE_PATH_JSON_HPP_

// The JSON form of a completed PointResult and the schema-1/4 warm candidate decoder.
// The field-by-field description is doc/raypath/raypath-cli-output.md.
//
// Numbers: every value is written as the shortest decimal that reads back to the same double
// (nlohmann's dump), so a seed round-trips bit-exactly; the three sun-grid arrays are first rounded
// to 9 significant digits, which bounds their size (a 720-row grid is a million cells per array) at
// a precision far below the kernel's own tolerances. NaN is written as null.

#include <string>
#include <vector>

#include "core/distribution.hpp"
#include "raypath/domain/error.hpp"
#include "raypath/point_result.hpp"

namespace lumice::raypath {

struct WarmCandidate;

// Significant digits kept for the sun-grid arrays (deviation_rad, entry_measure).
constexpr int kSunGridSignificantDigits = 9;

// The whole result as one JSON document (a UTF-8 string, no trailing newline). `product_version`
// is the Lumice version that produced it (generator.lumice).
std::string ToJson(const PointResult& result, const std::string& product_version);

// The warm seeds of an earlier output: every components[*].seed then every incomplete[*].seed,
// 9 row-major doubles each, appended to `seeds`. kInvalidArgument (with a message naming what is
// wrong) when `json_text` is not JSON, is not this format, carries another schema_version, or holds
// a seed that is not 9 numbers. Whether a seed is a rotation is the analysis's to decide, not this
// reader's.
Error ParseWarmSeeds(const std::string& json_text, std::vector<double>* seeds);

// Decode only: candidate identity and numeric facts are validated by survey_reuse.
Error DecodeWarmCandidate(const std::string& json_text, WarmCandidate* candidate);

// The config's own spelling of a distribution ("fixed" / "uniform" / "gauss" / "zigzag" /
// "laplacian" / "gauss_legacy"); a plain number in the config is "fixed". Shared by the point and
// probe meta blocks.
const char* DistributionName(DistributionType type);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SINGLE_PATH_JSON_HPP_
