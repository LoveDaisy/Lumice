#ifndef LUMICE_RAYPATH_DETAIL_SCHEMA3_NO_RELATED_FEATURE_HPP_
#define LUMICE_RAYPATH_DETAIL_SCHEMA3_NO_RELATED_FEATURE_HPP_

// schema3 report core, the no_related_feature dual-form ruling (scrum 666.2; scrum.md section 3
// contract verbatim — the rules are quoted, not reinvented):
//
//   The dual form: `outcome: "no_related_feature"` carries an outcome VALUE plus a `basis`
//   field. Rule A (exact zero spectral signal) outranks rule B (the certified smooth radiance).
//   Rule B is a CONDITIONED certificate whose four conditions, in scrum.md's own names, are:
//     1. partition 完备无 escape        — every support row's partition completed
//                                        (coverage kComplete; a refusal or an escape fails it);
//     2. 全对象 unlit-or-none           — the object set is empty, or every object's visibility
//                                        is kUnlit (unproven/partial/certified break it — the
//                                        fail-closed reading of plan A6: a default certificate
//                                        is unproven, and an unproven object breaks the arm);
//     3. 无 sufficient-ESS unattributed — the forward pass emitted no unattributed structure
//                                        (an unattributed structure at standing is the MC
//                                        contradicting the smooth-radiance claim);
//     4. S4 范围声明                    — the coverage declares the S4 branch boundary as
//                                        declared-not-produced (v1's certificate carries no
//                                        branch-boundary features; the declaration IS the scope
//                                        statement, read from the coverage table itself).
//   Rule 3 of the contract ("B v1 只对 2D 有效支撑签发") is an ADDITIONAL gate beside the four:
//   B issues only on `USupportKind::kArea` (the one 2D-support class); a restricted family
//   (kSpinOrbit and the other orbit kinds) does not issue — its absence is expressed by the
//   support block, not by this ruling.
//
// A refusal to issue is not a fallback outcome: the note names the reading (the document
// reports completed with an empty feature list, and an empty list does not prove absence).
// Every condition's verdict and its failure naming ride the ruling — the consumer sees WHICH
// leg blocked, not just a boolean.

#include <string>

#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/schema3/mc_attribution.hpp"
#include "raypath/results/report_attribution.hpp"

namespace lumice::raypath::schema3 {


const char* NoRelatedBasisName(NoRelatedBasis basis);


// The pure ruling. `unattributed` is the forward pass's outcome on the SAME core (the caller
// runs DeriveUnattributed first); `support_kind` is the declared measure's USupportKind;
// `zero_spectral_signal` is the v2 predicate's verdict (input_assembly's ZeroSpectralSignal —
// the one authority; this module consumes the boolean, the predicate stays with the v2 input
// types it reads).
NoRelatedRuling DeriveNoRelatedFeature(const Schema3DiscoveryCore& core, const UnattributedOutcome& unattributed,
                                       USupportKind support_kind, bool zero_spectral_signal);

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_DETAIL_SCHEMA3_NO_RELATED_FEATURE_HPP_
