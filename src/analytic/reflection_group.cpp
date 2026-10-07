#include "analytic/reflection_group.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace lumice::analytic {
namespace {

// numpy allclose's default tolerances (LI commutes_with_rotation_about calls np.allclose with no
// override): |a - b| <= 1e-8 + 1e-5 |b| elementwise.
constexpr double kAllCloseAtol = 1e-8;
constexpr double kAllCloseRtol = 1e-5;

// The D6 element as label arithmetic (LI symmetry/group.py Element): flip=false maps the ring
// index n -> n + shift (mod 6), flip=true maps n -> shift - n (mod 6). The six flips step their
// axes by 30 deg; the six rotations by 60 deg.
struct D6Element {
  bool flip;
  int shift;
};

constexpr D6Element kD6Elements[12] = {
  { false, 0 }, { false, 1 }, { false, 2 }, { false, 3 }, { false, 4 }, { false, 5 },
  { true, 0 },  { true, 1 },  { true, 2 },  { true, 3 },  { true, 4 },  { true, 5 },
};

// The ring a face number lives in: 3..8 (prism sides), 13..18 (upper pyramid), 23..28 (lower
// pyramid); the basal faces 1 / 2 are their own fixed points (LI _band). -1 outside the universe.
int RingBaseOf(int face) {
  if (face >= 3 && face <= 8) {
    return 3;
  }
  if (face >= 13 && face <= 18) {
    return 13;
  }
  if (face >= 23 && face <= 28) {
    return 23;
  }
  return -1;
}

// One element applied to one face number (LI apply): the ring index 3+i moves by the element's
// arithmetic mod 6, the base re-attaches it to the same ring, basal faces stay.
int ApplyFace(const D6Element& g, int face) {
  if (face == 1 || face == 2) {
    return face;
  }
  const int base = RingBaseOf(face);
  const int side = 3 + (face - base);  // the ring member as a side-face number, 3..8
  const int moved = g.flip ? (g.shift - side) : (side + g.shift);
  return base + ((moved - 3) % 6 + 6) % 6;
}

// The top-bottom swap B (LI _B_SWAP): basal faces swap, each pyramid face swaps with its partner
// on the other cone, prism sides stay.
int TopBottomSwap(int face) {
  if (face == 1 || face == 2) {
    return 3 - face;
  }
  if (face >= 13 && face <= 18) {
    return face + 10;
  }
  if (face >= 23 && face <= 28) {
    return face - 10;
  }
  return face;
}

}  // namespace

bool CommutesWithRotationAbout(const double m[9], const double axis[3], double probe_deg) {
  // Rodrigues' probe rotation R_a(theta) = I + sin t K + (1 - cos t) K^2 with K the skew matrix of
  // the unit axis (LI builds the same closed form; the eigen-decomposition alternative would be a
  // second authority for "the axis of a rotation" that BuildFoldScreen already owns).
  const double norm = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
  if (!(norm > 0.0)) {
    return false;  // LI raises ValueError; a zero axis has no rotations to commute with
  }
  const double k[3] = { axis[0] / norm, axis[1] / norm, axis[2] / norm };
  const double skew[9] = { 0.0, -k[2], k[1], k[2], 0.0, -k[0], -k[1], k[0], 0.0 };
  const double t = probe_deg * 3.14159265358979323846 / 180.0;
  const double s = std::sin(t);
  const double c = 1.0 - std::cos(t);
  double kk[9];  // K^2, row-major
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      kk[i * 3 + j] =
          skew[i * 3 + 0] * skew[0 * 3 + j] + skew[i * 3 + 1] * skew[1 * 3 + j] + skew[i * 3 + 2] * skew[2 * 3 + j];
    }
  }
  double rotation[9];
  for (int i = 0; i < 9; i++) {
    rotation[i] = (i % 4 == 0 ? 1.0 : 0.0) + s * skew[i] + c * kk[i];
  }
  double left[9];   // M R
  double right[9];  // R M
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      left[i * 3 + j] =
          m[i * 3 + 0] * rotation[0 * 3 + j] + m[i * 3 + 1] * rotation[1 * 3 + j] + m[i * 3 + 2] * rotation[2 * 3 + j];
      right[i * 3 + j] =
          rotation[i * 3 + 0] * m[0 * 3 + j] + rotation[i * 3 + 1] * m[1 * 3 + j] + rotation[i * 3 + 2] * m[2 * 3 + j];
    }
  }
  for (int i = 0; i < 9; i++) {
    if (!(std::fabs(left[i] - right[i]) <= kAllCloseAtol + kAllCloseRtol * std::fabs(right[i]))) {
      return false;
    }
  }
  return true;
}

bool PbdOrbit(const int* faces, int count, std::vector<std::vector<int>>* out) {
  for (int i = 0; i < count; i++) {
    if (faces[i] != 1 && faces[i] != 2 && RingBaseOf(faces[i]) < 0) {
      return false;
    }
  }
  std::vector<std::vector<int>> members;
  for (const D6Element& g : kD6Elements) {
    std::vector<int> image(count);
    std::vector<int> swapped(count);
    for (int i = 0; i < count; i++) {
      image[i] = ApplyFace(g, faces[i]);
      swapped[i] = TopBottomSwap(image[i]);
    }
    members.push_back(image);
    members.push_back(swapped);
  }
  std::sort(members.begin(), members.end());
  members.erase(std::unique(members.begin(), members.end()), members.end());
  *out = std::move(members);
  return true;
}

}  // namespace lumice::analytic
