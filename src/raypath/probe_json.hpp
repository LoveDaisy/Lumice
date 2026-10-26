#ifndef LUMICE_RAYPATH_PROBE_JSON_HPP_
#define LUMICE_RAYPATH_PROBE_JSON_HPP_

// The JSON form of a ProbeResult — the one serialization of it, which the `Lumice raypath probe`
// subcommand writes and a GUI shell will read through the same lumice_raypath.h entry point. The
// field-by-field description is doc/raypath/raypath-cli-output.md (schema 5). Fields are
// append-only from v1.

#include <string>

#include "raypath/probe_result.hpp"

namespace lumice::raypath {

// The whole result as one JSON document (a UTF-8 string, no trailing newline). `product_version`
// is the Lumice version that produced it (generator.lumice).
std::string ProbeToJson(const ProbeResult& result, const std::string& product_version);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PROBE_JSON_HPP_
