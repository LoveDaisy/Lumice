#ifndef RAYPATH_DETAIL_SURVEY_CONFIG_FINGERPRINT_H_
#define RAYPATH_DETAIL_SURVEY_CONFIG_FINGERPRINT_H_

// The routing-axis config fingerprint — the --warm contract's structural gate's one comparison
// input, with ONE implementation here that both sides call: the writer (the survey document's
// warm_index block, via survey_assembly) and the reader (the warm admission service). Two spellings would drift into a
// permanent false-mismatch red state (a56), so no caller serializes or hashes the projection itself.
//
// The axis set (the contract; a config field that starts to affect routing must be added to this
// projection and to the doc/raypath/raypath-cli-output.md axis table in the same change):
//   - the crystal entry: its full shape param (every scalar with its distribution) and its axis
//     distribution (all three slots) — the geometry and the measure the router reads;
//   - the request: the literal path layers and the symmetry bits (the scope the class expansion
//     runs under).
// Excluded on purpose: sun and wavelength do not participate in routing. A passing fingerprint
// admits a candidate and seeds, not its derived numeric facts: delta support and edge require the
// same wavelength, and band-sum values also require the same sun (and non-uniform line azimuth).
// Scene allocation and other renderer fields remain excluded from the routing projection.
//
// The value is the hex SHA256 of the projection's canonical JSON (sorted keys, no whitespace),
// prefixed with the axis-set tag so a projection change invalidates every earlier fingerprint
// instead of silently comparing across axioms.

#include <cstdint>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/domain/scope.hpp"

namespace lumice::raypath::survey {

// `path_layers` is the request's own layer encoding (the survey request's, already validated).
std::string RoutingConfigFingerprint(const ConfigManager& config, uint64_t crystal_id,
                                     const std::vector<std::vector<int>>& path_layers, uint8_t symmetry_bits);

// The fingerprint's own SHA-256 (FIPS 180-4, hex, lower case), exported so a known-answer test
// can pin the implementation to the algorithm the doc/raypath/raypath-cli-output.md contract
// promises ("hex SHA256") — the gate tests only prove differential behaviour, not correctness.
std::string Sha256Hex(const std::string& message);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_CONFIG_FINGERPRINT_H_
