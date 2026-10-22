#ifndef RAYPATH_DETAIL_SURVEY_CONFIG_FINGERPRINT_H_
#define RAYPATH_DETAIL_SURVEY_CONFIG_FINGERPRINT_H_

// The routing-axis config fingerprint — the --warm contract's structural gate's one comparison
// input, with ONE implementation here that both sides call: the writer (the survey document's
// warm_index block, via survey_assembly) and the reader (the point query's gate, via the C API
// bridge). Two spellings would drift into a permanent false-mismatch red state (a56), so no
// caller serializes or hashes the projection itself.
//
// The axis set (the contract; a config field that starts to affect routing must be added to this
// projection and to the doc/raypath/raypath-cli-output.md axis table in the same change):
//   - the crystal entry: its full shape param (every scalar with its distribution) and its axis
//     distribution (all three slots) — the geometry and the measure the router reads;
//   - the request: the literal path layers and the symmetry bits (the scope the class expansion
//     runs under).
// Excluded on purpose (the owner's routing ruling, routing.md ruling point 1): the sun position
// and the wavelength/spectrum — neither participates in routing, so a warm document stays
// structurally reusable across a moved sun or a changed lambda; both are re-checked per query
// against the document's scope receipt instead (the band-sum consumption gate). Also excluded:
// the crystal's scene-layer allocation and every other scene/renderer field — the router reads
// the crystal, the path and the scope, nothing else (the layer stack's only routing role is
// locating the crystal entry, which the crystal id already pins).
//
// The value is the hex SHA256 of the projection's canonical JSON (sorted keys, no whitespace),
// prefixed with the axis-set tag so a projection change invalidates every earlier fingerprint
// instead of silently comparing across axioms.

#include <cstdint>
#include <string>
#include <vector>

#include "config/config_manager.hpp"

namespace lumice::raypath::survey {

// The physical symmetry bits the scope runs under unless a caller names another: the survey
// command's default, the point query's inline-assembly scope, and the same query's fingerprint
// input are one semantic — this constant is its one spelling (three bare 7s would drift the day
// the default moves).
constexpr uint8_t kDefaultSymmetryBits = 7;

// `path_layers` is the request's own layer encoding (the survey request's, already validated).
std::string RoutingConfigFingerprint(const ConfigManager& config, uint64_t crystal_id,
                                     const std::vector<std::vector<int>>& path_layers, uint8_t symmetry_bits);

// The fingerprint's own SHA-256 (FIPS 180-4, hex, lower case), exported so a known-answer test
// can pin the implementation to the algorithm the doc/raypath/raypath-cli-output.md contract
// promises ("hex SHA256") — the gate tests only prove differential behaviour, not correctness.
std::string Sha256Hex(const std::string& message);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_CONFIG_FINGERPRINT_H_
