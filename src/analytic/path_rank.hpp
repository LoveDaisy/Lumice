#ifndef LUMICE_ANALYTIC_PATH_RANK_HPP_
#define LUMICE_ANALYTIC_PATH_RANK_HPP_

// Whether a face sequence is rank 0: every pose sends the light on along the incident direction, so
// the path's whole contribution is a point mass in the sun direction (LI docs/band-sum-contract.md
// section 5, geometry.halo_map_rank). The one criterion of this library and of the single-path
// analysis module (src/raypath/), which reads it for its point-mass outcome.
//
// The test is on the geometry, not on sampled poses: the fold matrix M = S_(m_k) ... S_(m_1) of the
// internal faces (S_n = I - 2 n n^T over each internal face's outward normal; the identity without
// internal reflections) equals I to kRankZeroFoldTolerance per entry, and the entry normal n_a and
// the unfolded exit normal -M^T n_b are parallel to kRankZeroWedgeToleranceDeg — a zero wedge. Then
// the outgoing direction is the incident one at every pose where the path is valid (a parallel-sided
// slab with M = I). Whether any pose is valid at all is a separate question.

#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

constexpr double kRankZeroFoldTolerance = 1e-12;
constexpr double kRankZeroWedgeToleranceDeg = 1e-9;

// `slots` resolved by ResolveFaceSequence, slot_count >= 2.
bool IsRankZeroPath(const FaceNormalTable& table, const int* slots, int slot_count);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_RANK_HPP_
