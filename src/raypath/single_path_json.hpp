#ifndef LUMICE_RAYPATH_SINGLE_PATH_JSON_HPP_
#define LUMICE_RAYPATH_SINGLE_PATH_JSON_HPP_

// The JSON form of a SinglePathResult — the one serialization of it, which the `Lumice raypath`
// subcommand writes and a GUI shell will read through the same lumice_raypath.h entry point — and its one
// reader, ParseWarmSeeds, which takes an earlier output back as warm starts. Writer and reader sit
// in one file so the keys they share cannot drift apart. The field-by-field description is
// doc/raypath/raypath-cli-output.md.
//
// Numbers: every value is written as the shortest decimal that reads back to the same double
// (nlohmann's dump), so a seed round-trips bit-exactly; the three sun-grid arrays are first rounded
// to 9 significant digits, which bounds their size (a 720-row grid is a million cells per array) at
// a precision far below the kernel's own tolerances. NaN is written as null.

#include <string>
#include <vector>

#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

// Significant digits kept for the sun-grid arrays (deviation_rad, entry_measure).
constexpr int kSunGridSignificantDigits = 9;

// The whole result as one JSON document (a UTF-8 string, no trailing newline). `product_version`
// is the Lumice version that produced it (generator.lumice).
std::string ToJson(const SinglePathResult& result, const std::string& product_version);

// The warm seeds of an earlier output: every components[*].seed then every incomplete[*].seed,
// 9 row-major doubles each, appended to `seeds`. kInvalidArgument (with a message naming what is
// wrong) when `json_text` is not JSON, is not this format, carries another schema_version, or holds
// a seed that is not 9 numbers. Whether a seed is a rotation is the analysis's to decide, not this
// reader's.
Error ParseWarmSeeds(const std::string& json_text, std::vector<double>* seeds);

// The two-family warm reader. A schema-1 document takes exactly ParseWarmSeeds's path (its seeds
// and nothing else — a schema-1 file is not a survey index). A schema-4 survey document goes
// through the consistency gates first — crystal id, path layers, the config fingerprint, and the
// family arm's measure echo when the document carries one — each refusal an
// `invalid_argument: warm index: ...` naming the gate; then the harvest fills `survey` and the
// seeds from the document's warm_index block. A document whose warm_index block is absent or
// missing any of its fields is NOT a refusal: `survey->index_present` stays false (the documented
// warm_index_absent degradation — no structural reuse, no seed consumption) and the query runs
// its context inline.
struct WarmGateContext {
  std::string config_fingerprint;  // RoutingConfigFingerprint's value for this request's config
  long long crystal_id = 0;
  std::vector<std::vector<int>> path_layers;
};

Error ParseWarmStart(const std::string& json_text, const WarmGateContext& gate, std::vector<double>* seeds,
                     SurveyWarmSource* survey);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SINGLE_PATH_JSON_HPP_
