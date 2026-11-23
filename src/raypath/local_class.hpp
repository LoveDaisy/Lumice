#ifndef LUMICE_RAYPATH_LOCAL_CLASS_HPP_
#define LUMICE_RAYPATH_LOCAL_CLASS_HPP_

// The local-class answer's VALUES (doc/raypath/raypath-elements.md's consumers): one row's
// physical classes at one sky direction, resolved by raypath/detail/local_class.hpp's resolver
// and carried across the workspace and C faces. Pure data — a frozen answer owns its storage
// and stays readable after the row scope and the workspace are gone.

#include <cstddef>
#include <optional>
#include <vector>

#include "core/crystal.hpp"
#include "raypath/point_result.hpp"
#include "raypath/single_path_analysis.hpp"  // the member evaluations' own point-chain knobs' defaults
#include "raypath/survey_reuse.hpp"

namespace lumice::raypath {

struct LocalClass {
  std::vector<int> representative;     // the class's lexicographic minimum
  std::vector<size_t> member_indices;  // into the row scope's member list, ascending
  // The members' own face sequences, same order as member_indices: the frozen answer is
  // self-contained (readable after the row scope and the workspace are gone).
  std::vector<std::vector<int>> member_sequences;
  std::vector<CrystalElement> to_representative;  // per member: g with g·member == representative
};

struct LocalClassRequest {
  // The sky point the classes are resolved at (the click), and the scene sun's azimuth in
  // degrees. Both spellings of a vertical plane's azimuth (φ and φ + 180) are accepted: the
  // sky-view predicate and the compensation mirror are invariant under it.
  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
  double sun_azimuth_deg = 0.0;
};

// The row + sky point a workspace resolves classes for: the analysis list row's own selection
// spelling. The scene sun's azimuth is the workspace's to read, not the caller's to restate.
struct LocalClassQuery {
  IdType crystal_id = 0;
  std::vector<int> representative;
  uint8_t symmetry_bits = 0;
  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
};

// The engine-side interpretive-point query (the adjudicated answer form): the row + the click
// in, the interpretive class and the selected member's own point payload out, resolution and
// member evaluation settled within ONE query deadline. Consumers — GUI and CLI alike — read
// this form; neither re-assembles classes with separately-run member results.
struct InterpretivePointQuery {
  IdType crystal_id = 0;
  std::vector<int> representative;  // the analysis row's representative sequence
  uint8_t symmetry_bits = 0;        // the row's P/B/D bits
  double target_altitude_deg = 0.0;
  double target_azimuth_deg = 0.0;
  // 0 = the point chain's documented default; the one deadline the whole query settles under.
  int budget_ms = 0;
  const volatile int* interrupted = nullptr;
  // The member evaluations' own point-chain knobs (the point face's, verbatim per member).
  int sample_count = kDefaultSampleCount;
  int sun_grid_lat_count = kDefaultSunGridLatCount;
  // Optional explicit warm candidate (the point face's contract): a member evaluation admits it
  // only through the existing single admission authority, so a candidate naming another member
  // is an automatic miss for every other member and a strict refusal where that contract says so.
  std::optional<WarmCandidate> warm_candidate;
  // Non-empty: evaluate exactly this member — a row member, refused otherwise. No automatic
  // selection runs: the fixed member keeps its own geometry and log density facts and is never
  // auto-swapped (the fixed-member contract).
  std::vector<int> fixed_member;
};

// The interpretive-point answer: one frozen value owning its storage, readable after the row
// scope and the workspace are gone.
struct InterpretivePointAnswer {
  // The row's classes at the sky point (the resolver's fixed representative order). Selection
  // never changes these facts — a cancelled or deadline-stopped member search leaves them
  // standing (the algebra is not the search's to invalidate).
  std::vector<LocalClass> classes;
  // Where the interpretation landed: the class index into `classes` and the member index into
  // that class's member_indices. -1/-1 = no evaluated member's interpretive pose carried
  // positive linear weight (rho > 0 — the acceptance wording's non-zero rho; a deep tail keeps
  // its finite log but reads exactly 0 in the linear form) — "this search has not found one"
  // (the completeness wording, not a noise wording); `point` then holds the FIRST class's
  // representative's own evaluation, the deterministic anchor the consumer displays alongside
  // the honest state.
  int selected_class = -1;
  int selected_member = -1;
  bool member_fixed = false;  // the caller fixed the member; no automatic selection ran
  // The selected member's own payload; with no selection, the first class's representative's.
  PointResult point;
};

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_LOCAL_CLASS_HPP_
