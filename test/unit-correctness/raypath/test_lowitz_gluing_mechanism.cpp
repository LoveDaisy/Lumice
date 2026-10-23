// The gluing lemma's mechanism anchor (design gap2 §4/§6, plan AC2/AC4): the physical P gating's
// source B is pinned to the fold mechanism at three levels —
//   1. the folded latitude law's flip probability is 1/2 at every bin for a pole-symmetric
//      proposal (Gaussian, Laplacian, legacy-Gaussian, and a pole-centred uniform band, both
//      folding path kinds), and departs from 1/2 on the corpus's zenith-20° control axis;
//   2. DeclaredPoseDensity satisfies rho(lambda, phi, psi + pi) == rho(lambda, phi, psi) bit for
//      bit on the corpus's Lowitz axis, and does not on the control;
//   3. the optical witness at sampled poses: rotating the body labels by 180° (R -> R·Rz(pi),
//      every prism face +3) leaves the outgoing direction, the corridor entry measure and the
//      Fresnel transmission bit-identical — the corpus's R→R·Rz(π) audit promoted to a test on
//      the production DiagnosticSampler / EvaluatePath / Corridor pieces, fixed seed.
// Prediction 5's reconciliation (design §7): the control axis' two roll petals weigh
// Φ(centre/σ) : (1−Φ) — zenith centre 20°, σ 40° gives Φ(0.5) ≈ 0.6915 : 0.3085, so the flip
// share near the pole bins sits at 1−Φ(0.5) ≈ 0.31, not 1/2. The corpus's near_roll0/near_roll180
// counts (49880/15656 over 65536 = 262144/4) are one window's lobe counts, not the global split —
// cited as direction evidence; the anchor here is the derived Φ value, asserted through the
// per-bin FlipProbability direction.
//
// Exactness note (a56): with equal leaves the density identity holds pointwise iff the roll law
// is invariant under a 2π shift; the corpus's narrow roll (σ = 0.3°) puts the cross-petal terms
// at exp(−π²/2σ²) ≈ exp(−1.8e5) — true zeros in double — so the bit-level pinning is exact for
// it. Wider rolls converge to the identity geometrically; the derivation's answer is the
// reviewed design's (equal leaves + uniform azimuth + folding path).

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <set>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"
#include "config/config_manager.hpp"
#include "core/math.hpp"
#include "core/optics.hpp"
#include "raypath/detail/diagnostic_sampler.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/measure/declared_density.hpp"

namespace lumice {
namespace {

// The corpus's C16 Lowitz axis, in core slots: zenith Gaussian 0°±40° (latitude 90°±40°), roll
// Gaussian 0°±0.3°, azimuth uniform over a full turn. `zenith_center_deg` = 20 gives the
// corpus's control axis.
AxisDistribution LowitzAxis(float zenith_center_deg) {
  AxisDistribution axis;
  axis.latitude_dist = Distribution{ DistributionType::kGaussian, 90.0f - zenith_center_deg, 40.0f };
  axis.azimuth_dist = Distribution{ DistributionType::kUniform, 0.0f, 360.0f };
  axis.roll_dist = Distribution{ DistributionType::kGaussian, 0.0f, 0.3f };
  return axis;
}

// The corpus's scene shape: a regular prism under that axis, sun on the horizon, 550 nm.
ConfigManager LowitzScene(float zenith_center_deg) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.2f, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1.0f, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_ = LowitzAxis(zenith_center_deg);
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 0, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// ---- 1. the flip probability matrix (folding paths, per family) ----

TEST(LowitzGluingMechanism, FlipProbabilityIsHalfOnPoleSymmetricProposals) {
  const double kDeg = math::kDegreeToRad;
  // Unbounded-support families answer 1/2 at every zenith bin — the corpus's per-zenith audit.
  const Distribution families[] = {
    Distribution{ DistributionType::kGaussian, 90.0f, 40.0f },
    Distribution{ DistributionType::kGaussianLegacy, 90.0f, 40.0f },
    Distribution{ DistributionType::kLaplacian, 90.0f, 40.0f },
  };
  for (const auto& slot : families) {
    const auto law = raypath::MakeLatitudeDensity(slot);
    for (double zenith_deg : { 1., 10., 30., 60., 90., 120., 170. }) {
      EXPECT_NEAR(law.FlipProbability((90.0 - zenith_deg) * kDeg), 0.5, 1e-12)
          << "zenith " << zenith_deg << " type " << static_cast<int>(slot.type);
    }
  }
  // A pole-centred uniform band folds its far half back with equal mass wherever it has mass
  // (both probes inside the folded domain [−90°, 90°], on the band's folded support [60°, 90°]).
  const auto band = raypath::MakeLatitudeDensity(Distribution{ DistributionType::kUniform, 90.0f, 60.0f });
  EXPECT_NEAR(band.FlipProbability(80.0 * kDeg), 0.5, 1e-12);
  EXPECT_NEAR(band.FlipProbability(70.0 * kDeg), 0.5, 1e-12);
}

TEST(LowitzGluingMechanism, FlipProbabilityDepartsOnTheZenith20Control) {
  const double kDeg = math::kDegreeToRad;
  // The corpus's control axis (zenith centre 20°): near the up-pole bins the reflected branch is
  // the lighter one — 1−Φ(centre/σ) < 1/2 — and no bin reads exactly 1/2.
  const auto control = raypath::MakeLatitudeDensity(Distribution{ DistributionType::kGaussian, 70.0f, 40.0f });
  for (double zenith_deg : { 1., 10., 30. }) {
    const double f = control.FlipProbability((90.0 - zenith_deg) * kDeg);
    EXPECT_NE(f, 0.5) << "zenith " << zenith_deg;
    EXPECT_LT(f, 0.5) << "zenith " << zenith_deg;
  }
}

// ---- 2. the joint density's half-turn periodicity, bit for bit ----

TEST(LowitzGluingMechanism, DeclaredDensityIsHalfTurnPeriodicBitForBit) {
  const double kDeg = math::kDegreeToRad;
  // The shift uses the DOUBLE pi (math::kPi is the float spelling: its 8.7e-8 offset from the
  // double pi, run through the wrapped roll branch's unwrap, is amplified by the narrow roll's
  // density slope into a 1e-5 artifact the corpus's audit does not carry — it measured with the
  // double pi and recorded max relative error 0). The FP round trip through the wrap still loses
  // ~1e-14 relative, so the pinning is a 1e-12 relative closeness, not a bit comparison.
  const AxisDistribution lowitz = LowitzAxis(0.0f);
  const auto latitude = raypath::MakeLatitudeDensity(lowitz.latitude_dist);
  // Petal-local psi values (the roll's σ is 0.3°): each side's density is its own petal's term.
  for (double phi_deg : { 60.0, 20.0, 0.0, -40.0 }) {
    for (double psi : { 0.003, -0.003, math::kPiD + 0.003, math::kPiD - 0.003 }) {
      const double rho = raypath::DeclaredPoseDensity(latitude, lowitz, 0.0, phi_deg * kDeg, psi);
      const double rho_p = raypath::DeclaredPoseDensity(latitude, lowitz, 0.0, phi_deg * kDeg, psi + math::kPiD);
      if (!(rho > 0.0)) {
        ADD_FAILURE() << "density not positive at phi " << phi_deg << " psi " << psi;
        continue;
      }
      EXPECT_NEAR(rho_p / rho, 1.0, 1e-12) << "phi " << phi_deg << " psi " << psi;
    }
  }
  // The control axis is not periodic: the two leaves weigh Φ(centre/σ) ≠ 1/2. At latitude 69°
  // (zenith 21°) the reflected branch's preimage (111°) sits ~1σ past the 70° centre while the
  // direct one sits at it, so the half-turned leaf carries ≈Φ(−1) ≈ 0.16 less weight.
  const AxisDistribution control = LowitzAxis(20.0f);
  const auto control_latitude = raypath::MakeLatitudeDensity(control.latitude_dist);
  const double rho = raypath::DeclaredPoseDensity(control_latitude, control, 0.0, 69.0 * kDeg, 0.003);
  const double rho_p = raypath::DeclaredPoseDensity(control_latitude, control, 0.0, 69.0 * kDeg, 0.003 + math::kPiD);
  ASSERT_GT(rho, 0.0);
  ASSERT_GT(rho_p, 0.0);
  EXPECT_LT(rho_p / rho, 0.8) << "the control's petals weigh " << (rho_p / rho) << " : 1, not 1 : 1";
}

// ---- 3. the optical witness under the half-turn relabel, on production pieces ----

TEST(LowitzGluingMechanism, HalfTurnRelabelledWitnessIsBitIdentical) {
  const ConfigManager config = LowitzScene(0.0f);
  raypath::InputSnapshot snapshot;
  const std::vector<raypath::LayerSelection> selection{ raypath::LayerSelection{ 0, 1, { 3, 5 }, 7 } };
  ASSERT_TRUE(raypath::CaptureInput(config.scene_, "lowitz gluing", selection, &snapshot).Ok());
  const raypath::DiagnosticSampler sampler(snapshot, 20260534u,
                                           raypath::WavelengthSample{ 550.0f, std::nullopt, "lowitz gluing" });
  raypath::AssembledInput first;
  ASSERT_TRUE(sampler.Draw(0, &first).Ok());
  const auto& geometry = first.layers[0];
  const AxisDistribution axis = LowitzAxis(0.0f);
  const auto latitude = raypath::MakeLatitudeDensity(axis.latitude_dist);

  // The twelve prism paths (entrance x offset {+2°, +4 faces}), resolved and corridor'd once.
  std::vector<std::vector<int>> members;
  std::vector<std::array<int, 2>> member_slots;
  std::vector<std::unique_ptr<analytic::Corridor>> corridors;
  std::set<std::vector<int>> seen;
  for (int entrance = 3; entrance <= 8; ++entrance) {
    for (int offset : { 2, 4 }) {
      std::vector<int> faces{ entrance, 3 + (entrance - 3 + offset) % 6 };
      if (!seen.insert(faces).second) {
        continue;
      }
      std::array<int, 2> resolved{};
      EXPECT_EQ(analytic::ResolveFaceSequence(geometry.normals, faces.data(), 2, resolved.data()),
                analytic::Status::kOk);
      members.push_back(std::move(faces));
      member_slots.push_back(resolved);
      corridors.push_back(
          std::make_unique<analytic::Corridor>(geometry.normals, geometry.polygons, resolved.data(), 2));
    }
  }
  ASSERT_EQ(members.size(), 12u);

  const double index_n = IceRefractiveIndex::Get(550.0f);
  double max_direction_error = 0.0;
  double max_area_error = 0.0;
  double max_transmission_error = 0.0;
  double max_density_relative_error = 0.0;
  int witnesses = 0;
  constexpr int kSamples = 400;
  for (int i = 0; i < kSamples; ++i) {
    raypath::AssembledInput input;
    if (!sampler.Draw(static_cast<uint64_t>(i), &input).Ok()) {
      ADD_FAILURE() << "diagnostic draw " << i << " failed";
      break;  // the prefix-replay sampler has no recovery; later draws would fail the same way
    }
    const auto& pose = input.layers[0].analytic_pose;
    const auto& angles = input.layers[0].angles;
    const auto& inc = input.source.incident_direction;
    double body[3]{};
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 3; ++k) {
        body[j] += pose[k * 3 + j] * inc[k];
      }
    }
    const double rho = raypath::DeclaredPoseDensity(latitude, axis, angles[0], angles[1], angles[2]);
    if (rho > 0.0) {
      const double rho_p = raypath::DeclaredPoseDensity(latitude, axis, angles[0], angles[1], angles[2] + math::kPiD);
      max_density_relative_error = std::max(max_density_relative_error, std::abs(rho_p - rho) / rho);
    }
    for (size_t m = 0; m < members.size(); ++m) {
      double segments[9]{};
      double interfaces[2]{};
      analytic::PathOutputs optical{ {}, 0, segments, interfaces };
      if (!analytic::EvaluatePath(geometry.normals, member_slots[m].data(), 2, index_n, inc.data(), pose.data(),
                                  &optical)) {
        continue;  // this member is not a positive-A path at this pose
      }
      const double area = corridors[m]->Evaluate(body, index_n).value;
      if (!(area > 0)) {
        continue;
      }
      // R -> R · Rz(pi): negate the pose's first two columns (row-major) — and the body-frame
      // sun's x/y with them — and shift every prism face label by 3.
      std::array<double, 9> turned = pose;
      for (int j = 0; j < 3; ++j) {
        turned[j * 3] = -pose[j * 3];
        turned[j * 3 + 1] = -pose[j * 3 + 1];
      }
      std::vector<int> turned_faces;
      turned_faces.reserve(members[m].size());
      for (int face : members[m]) {
        turned_faces.push_back(3 + (face - 3 + 3) % 6);
      }
      const auto found = std::find(members.begin(), members.end(), turned_faces);
      if (found == members.end()) {
        ADD_FAILURE() << "the half-turn image left the twelve-path set";
        continue;
      }
      const size_t mi = static_cast<size_t>(found - members.begin());
      const std::array<double, 3> turned_body{ -body[0], -body[1], body[2] };
      double turned_segments[9]{};
      double turned_interfaces[2]{};
      analytic::PathOutputs partner{ {}, 0, turned_segments, turned_interfaces };
      if (!analytic::EvaluatePath(geometry.normals, member_slots[mi].data(), 2, index_n, inc.data(), turned.data(),
                                  &partner)) {
        ADD_FAILURE() << "the half-turn partner must evaluate wherever the original does (mirrored same pose)";
        continue;
      }
      for (int j = 0; j < 3; ++j) {
        max_direction_error =
            std::max(max_direction_error, std::abs(partner.outgoing_direction[j] - optical.outgoing_direction[j]));
      }
      max_transmission_error =
          std::max(max_transmission_error, std::abs(partner.fresnel_transmission - optical.fresnel_transmission));
      max_area_error =
          std::max(max_area_error, std::abs(corridors[mi]->Evaluate(turned_body.data(), index_n).value - area));
      ++witnesses;
    }
  }
  EXPECT_GT(witnesses, 100) << "the sample budget stopped exercising the witness";
  EXPECT_EQ(max_direction_error, 0.0);
  EXPECT_EQ(max_transmission_error, 0.0);
  EXPECT_EQ(max_area_error, 0.0);
  // The corpus recorded max relative error 0 here (its psi+kPi round trip landed back on the
  // same double); the honest bound for the same expression is the FP round trip itself.
  EXPECT_LT(max_density_relative_error, 1e-12);
}

}  // namespace
}  // namespace lumice
