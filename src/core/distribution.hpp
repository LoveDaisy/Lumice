#ifndef LUMICE_CORE_DISTRIBUTION_HPP_
#define LUMICE_CORE_DISTRIBUTION_HPP_


namespace lumice {

enum class DistributionType {
  kNoRandom,
  kUniform,
  kGaussian,
  kZigzag,
  kLaplacian,
  kGaussianLegacy,  // Gaussian without Jacobian correction (for reproducing legacy simulation results)
};


//! @brief A one-dimensional random distribution described by two positional parameters.
//!
//! @details The two data members are named for the *role* each slot plays — a role that is
//!   invariant across every DistributionType — rather than for a statistical quantity that is
//!   only accurate for the Gaussian family. The former names (`mean` / `std`) claimed a meaning
//!   that holds for 2 of the 6 types and misleads for the other 4.
//!
//!   - `center` — the anchor / location of the distribution.
//!   - `spread` — the width / amplitude of the distribution.
//!
//!   Per-type reading of the two slots:
//!
//!   | type            | center             | spread                      |
//!   |-----------------|--------------------|-----------------------------|
//!   | kNoRandom       | the value itself   | unused                      |
//!   | kUniform        | interval midpoint  | **full** range (not half)   |
//!   | kGaussian       | mean μ             | standard deviation σ        |
//!   | kGaussianLegacy | mean μ             | standard deviation σ        |
//!   | kZigzag         | tilt offset B      | amplitude A                 |
//!   | kLaplacian      | location μ         | scale b                     |
//!
//!   Call sites that know the type should use the named accessors below instead of reading the
//!   members directly — the accessor name states which row of the table is being relied on, and
//!   asserts the type actually matches. Call sites that are genuinely type-erased (JSON
//!   serialization, GPU parameter packing, hashing, equality) read `center` / `spread` directly;
//!   that is the intended use of the neutral names, not a workaround.
//!
//!   Aggregate brace-initialization `Distribution{ type, center, spread }` remains valid: the
//!   slot order is the same for every type, so positional construction cannot silently swap
//!   meanings.
struct Distribution {
  DistributionType type;
  float center;
  float spread;

  //! @brief The constant value produced by a kNoRandom distribution.
  float Value() const;

  //! @brief Midpoint of a kUniform interval.
  float UniformCenter() const;
  //! @brief Full width of a kUniform interval (NOT the half-width).
  float UniformFullRange() const;

  //! @brief Mean μ of a kGaussian / kGaussianLegacy distribution.
  float Mean() const;
  //! @brief Standard deviation σ of a kGaussian / kGaussianLegacy distribution.
  float Std() const;

  //! @brief Tilt offset B of a kZigzag distribution.
  float Tilt() const;
  //! @brief Amplitude A of a kZigzag distribution.
  float Amplitude() const;

  //! @brief Location μ of a kLaplacian distribution.
  float Location() const;
  //! @brief Scale b of a kLaplacian distribution.
  float Scale() const;
};

//! @brief Field-by-field value equality for two Distributions.
//!
//! @details The single owner of "are these two distributions the same". Both
//!   config_compare.hpp's operator==(Distribution, Distribution) — the
//!   re-simulation trigger predicate — and the sync-group leader normalization in
//!   crystal_config.cpp route here, so adding a field to Distribution cannot
//!   leave one of them silently comparing the old subset. The static_assert is
//!   what forces that update.
inline bool DistributionValueEqual(const Distribution& a, const Distribution& b) {
  static_assert(sizeof(Distribution) == 12, "Update DistributionValueEqual when Distribution fields change");
  return a.type == b.type && a.center == b.center && a.spread == b.spread;
}


struct AxisDistribution {
  AxisDistribution();

  //! @brief Check if this distribution represents full-sphere uniform sampling.
  //! @details Checks all three of azimuth, latitude AND roll. Roll is not decorative here: the
  //!   fast path this predicate dispatches to never applies the pole-crossing `roll += π`
  //!   correction that the general LatLut path applies on a flip, so the two paths only agree
  //!   when roll is invariant under that +180° shift. See IsRollRotationallySymmetric.
  //! @warning The parameterized SampleSphericalPointsSph does NOT include the asin(u) Jacobian correction.
  //!   For full-sphere uniform, the caller must use the parameter-less overload instead.
  bool IsFullSphereUniform() const;

  //! @brief Check if azimuth is uniformly distributed over the full 360°.
  //! @details Only checks the azimuth type and its full range; latitude and roll are ignored.
  bool IsAzRotationallySymmetric() const;

  //! @brief Check whether roll is invariant under the pole-crossing +180° correction — i.e.
  //!   sampling roll and sampling roll+π yield the same distribution.
  //! @details This is what makes the kFullSphere fast path a provable optimization rather than a
  //!   semantic fork: detail::NormalizeLatitude reflects a latitude past the pole and reports a
  //!   `flip`, on which the general path adds π to BOTH azimuth and roll (Rz(roll) has period 2π),
  //!   while the fast path adds nothing at all. Equal distributions therefore require roll's own
  //!   distribution to be unchanged by a +π shift.
  //! @warning The condition implemented below — kUniform spanning a full 360° — is STRICTER than
  //!   the mathematical requirement. The requirement is only "invariant under a +180° shift"; a
  //!   full-period uniform is merely the one non-degenerate form among the six DistributionTypes
  //!   this repo has today that satisfies it (which is also why, like
  //!   IsAzRotationallySymmetric, it need not check the center: a full-period uniform is shift
  //!   invariant regardless of where it is centered). If a new DistributionType is added, do not
  //!   copy this implementation mechanically — ask whether the new type can be +180°-invariant
  //!   without being a full-range uniform (e.g. a symmetric two-lobe form), and widen the
  //!   predicate rather than assuming uniform is the only answer.
  bool IsRollRotationallySymmetric() const;

  //! @brief Check whether every one of azimuth/latitude/roll is kNoRandom — i.e. this axis
  //!   never consumes the RNG and yields one fixed orientation for every ray.
  //! @details The orientation-side counterpart of IsDeterministic(const CrystalParam&)
  //!   (trace_ops.hpp), and deliberately NOT the same predicate: shape and axis are
  //!   independent axes of a crystal setting, and conflating them is exactly the gap this
  //!   predicate closes — the commonest halo setup (fixed shape + random orientation) is
  //!   deterministic on the shape axis and stochastic on this one.
  //! @note It needs no !IsFullSphereUniform() guard: that predicate requires both
  //!   azimuth_dist and latitude_dist to be kUniform, so a full-sphere axis can never satisfy
  //!   the all-kNoRandom test below. The two are mutually exclusive by construction and an
  //!   added guard would be a dead conjunct.
  //! @note What AxisDeterminismMatchesRuntimeOrientation (test_simulator.cpp) pins, and what it
  //!   does not. It pins the load-bearing claim: an all-kNoRandom axis really does yield one
  //!   bit-identical rotation for every ray, and a stochastic one really does vary. That is what
  //!   the sample count depends on, and it is invisible to a truth-table test — a change making
  //!   kNoRandom consume the RNG turns the pin red while every truth-table case stays green
  //!   (verified by mutation). It does NOT pin which branch InitRay_rot takes.
  //! @note This comment used to claim the two sampler paths were "statistically equivalent", on
  //!   the strength of a measurement of the fraction of |world z| < 0.5 (0.5000 vs 0.4977 over 20k
  //!   samples), and told the reader not to add a test claiming otherwise. That claim was wrong and
  //!   the measurement could not have caught it: |world z| reads the sampled AXIS DIRECTION only,
  //!   and the difference between the two paths lives entirely in roll — the fast path drops the
  //!   pole-crossing `roll += π` that the general path applies on a flip. A yardstick that cannot
  //!   see the quantity under test reports agreement no matter what (a16). The two paths are now
  //!   made equivalent by construction instead: IsFullSphereUniform requires
  //!   IsRollRotationallySymmetric, so the fast path is only ever taken where the correction is a
  //!   no-op in distribution. RollFlipDivergence.* (test_simulator.cpp) measures roll directly and
  //!   pins that.
  bool IsAxisDeterministic() const;

  Distribution azimuth_dist;
  Distribution latitude_dist;
  Distribution roll_dist;
};

}  // namespace lumice

#endif  // LUMICE_CORE_DISTRIBUTION_HPP_
