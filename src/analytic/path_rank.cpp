#include "analytic/path_rank.hpp"

#include <cmath>

namespace lumice::analytic {

bool IsRankZeroPath(const FaceNormalTable& table, const int* slots, int slot_count) {
  // M = S_(m_k) ... S_(m_1), applied in path order: M <- S_n M.
  double m[3][3] = { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } };
  for (int k = 1; k + 1 < slot_count; k++) {
    const double* n = table.normal[slots[k]];
    double next[3][3];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        next[i][j] = m[i][j] - 2.0 * n[i] * (n[0] * m[0][j] + n[1] * m[1][j] + n[2] * m[2][j]);
      }
    }
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        m[i][j] = next[i][j];
      }
    }
  }
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (!(std::fabs(m[i][j] - (i == j ? 1.0 : 0.0)) <= kRankZeroFoldTolerance)) {
        return false;
      }
    }
  }
  // angle(n_a, -M^T n_b); M is I to 1e-12 here, and the formula is kept whole so that the tolerance
  // reads as the contract states it.
  const double* na = table.normal[slots[0]];
  const double* nb = table.normal[slots[slot_count - 1]];
  double unfolded[3];
  for (int j = 0; j < 3; j++) {
    unfolded[j] = -(m[0][j] * nb[0] + m[1][j] * nb[1] + m[2][j] * nb[2]);
  }
  // The angle by atan2(|a x b|, a . b), accurate at zero where acos is not.
  const double cross[3] = { na[1] * unfolded[2] - na[2] * unfolded[1], na[2] * unfolded[0] - na[0] * unfolded[2],
                            na[0] * unfolded[1] - na[1] * unfolded[0] };
  const double dot = na[0] * unfolded[0] + na[1] * unfolded[1] + na[2] * unfolded[2];
  const double angle = std::atan2(std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]), dot);
  return angle <= kRankZeroWedgeToleranceDeg * 3.14159265358979323846 / 180.0;
}

}  // namespace lumice::analytic
