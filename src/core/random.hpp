#ifndef CORE_RANDOM_H_
#define CORE_RANDOM_H_

// Probability models and RNG engines, split out of core/math.hpp (which keeps the
// linear algebra / geometry solvers / scalar helpers). The two families move
// together because their interfaces are coupled — RandomNumberGenerator::Get takes
// a Distribution by value, RandomSampler::SampleSphericalPointsSph takes an
// AxisDistribution — so extracting one alone would leave random.hpp including
// math.hpp anyway. Splitting here also drops <nlohmann/json.hpp> (and <random>)
// from every math.hpp consumer that does not touch distributions.

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <random>
#include <string>

namespace lumice {

struct AxisDistribution;

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

namespace detail {

//! @brief Map a uniform draw `u` to an index in `[0, n)`, clamping the upper boundary.
//! @details The single owner of "turn a GetUniform() draw into a subscript". Split out of
//!   RandomNumberGenerator::GetUniformIndex as a pure function (touching no RNG state) so the
//!   boundary case can be unit-tested by direct injection instead of by drawing until the
//!   boundary happens to come up — the same shape detail::RandomSampleSelectBin (geo3d.cpp)
//!   uses for its own boundary logic.
//! @note The clamp is `>=`, not `== n`, and that is load-bearing. `std::uniform_real_distribution`
//!   is specified over the half-open `[0, 1)` but three standard libraries were measured at
//!   6e9 draws each (2026-09-04): libc++ (Apple clang, arm64) returns exactly `1.0f` at a rate
//!   of ~3.1e-8, while MSVC STL 19.44 and libstdc++ 13.3 returned it zero times. On `u == 1.0f`
//!   the product `u * (float)n` overshoots by more than one whenever `n > 2^24` and `(size_t →
//!   float)` itself rounds up — measured `n = 1073743824` lands 48 past the end — so an `== n`
//!   special case would silently miss exactly those inputs. `u < 1` is structurally safe
//!   (exhaustively verified for `n = 1..2^25` plus large-`n` fixed points on all three
//!   platforms), so `u == 1.0f` is the only input the clamp ever engages on.
//! @warning The measurement above is a property of the standard library implementation, not of
//!   the C++ standard. Changing toolchain or platform does not make the clamp unnecessary; it
//!   only changes which implementations reach it.
//! @param u a draw from RandomNumberGenerator::GetUniform(); values outside [0, 1] are not expected
//! @param n the exclusive upper bound; must be > 0. `n == 0` calls lumice::FatalAbort (it is a
//!   hard guard, not an assert, so it holds under NDEBUG too): no subscript is correct for an
//!   empty range, and the pre-guard body underflowed `n - 1` into SIZE_MAX and returned it.
size_t ClampUniformToIndex(float u, size_t n);

}  // namespace detail

class RandomNumberGenerator {
 public:
  explicit RandomNumberGenerator(uint32_t seed);

  float GetGaussian();

  //! @brief Draw a uniform float. Nominally `[0, 1)` — but see the warning.
  //! @warning If you are about to use the result as a subscript (`(size_t)(GetUniform() * n)`),
  //!   use GetUniformIndex(n) instead. Some standard library implementations return exactly
  //!   `1.0f` (measured on libc++, see detail::ClampUniformToIndex), which puts that expression
  //!   one — or, for large `n`, several — slots past the end. Do not hand-write
  //!   `if (idx >= n) idx = n - 1` at the call site; that convention is what left this repo with
  //!   guards on the GPU paths and none on the CPU ones.
  float GetUniform();

  //! @brief Draw a uniform index in `[0, n)`. The single owner of the GetUniform()-to-subscript
  //!   conversion; see detail::ClampUniformToIndex for the boundary contract and the measurement
  //!   behind it.
  //! @details Consumes exactly one GetUniform() draw, so substituting it for a hand-written
  //!   `(size_t)(GetUniform() * n)` leaves the RNG draw order and draw count bit-for-bit unchanged.
  //! @param n the exclusive upper bound; must be > 0
  size_t GetUniformIndex(size_t n);

  float Get(Distribution dist);
  void Reset();
  void SetSeed(uint32_t seed);

  static RandomNumberGenerator& GetInstance();

 private:
  uint32_t seed_;
  std::mt19937 generator_;
  std::normal_distribution<float> gauss_dist_;
  std::uniform_real_distribution<float> uniform_dist_;
};

struct LatLut;  // core/lat_lut.hpp — prebuilt inverse-CDF table for kLutInverseCdf sampling.

class RandomSampler {
 public:
  /*! @brief Generate points distributed uniformly on sphere, in spherical form, (lon, lat).
   *
   * @param data output data, (lon, lat), in rad
   * @param num
   */
  static void SampleSphericalPointsSph(float* data, size_t num = 1, size_t step = 3);

  /*! @brief Generate points distributed on sphere surface up to latitude, with roll, in spherical form, (lon, lat,
   * roll).
   *
   * @param axis_dist axis distribution, including information of zenith / azimuth / roll.
   * @param data output data, (lon, lat, roll), in rad. Roll is sampled from axis_dist.roll_dist and
   *   adjusted by +π when a latitude fold (pole crossing) occurs, keeping roll coupled to the fold.
   * @param num number of points.
   * @param lat_lut optional prebuilt inverse-CDF LUT (330.2). When SelectLatPath routes the
   *   latitude distribution to kLutInverseCdf, the caller builds the LUT ONCE (per axis
   *   distribution, amortized — never per ray) and passes it here. Ignored for other paths;
   *   must be non-null when the path is kLutInverseCdf.
   * @note Caller must provide a buffer of at least 3×num floats.
   */
  static void SampleSphericalPointsSph(const AxisDistribution& axis_dist, float* data, size_t num = 1,
                                       const LatLut* lat_lut = nullptr);

  RandomSampler() = delete;
};

namespace detail {

//! @brief Whether a distribution over an angle is unchanged by an arbitrary constant rotation —
//!   i.e. kUniform spanning a full turn.
//! @details The shared primitive under AxisDistribution::IsAzRotationallySymmetric and
//!   ::IsRollRotationallySymmetric, and the one place the 360° tolerance is spelled. Stated over
//!   the two raw fields it reads rather than over a Distribution, so a caller holding the fields
//!   but not the struct can ask without assembling a stand-in object whose unread members are a
//!   contract no compiler checks. Its direct callers are the two AxisDistribution members below
//!   and detail::IsDApplicableParams (crystal.cpp); the reason the raw-field form exists at all
//!   is two hops further out — LUMICE_IsDApplicable (c_api_editor.cpp) reaches it through
//!   IsDApplicableParams, and the GUI consults that instead of keeping its own copy of this rule.
//!   `full_range_deg` is read type-erased (the raw `Distribution::spread`, not
//!   UniformFullRange()) because it is passed unconditionally, before the type has been
//!   established.
//! @note Deliberately says nothing about the center: a full-period uniform is rotation invariant
//!   wherever it is centered.
//! @note Lives in `detail` for the same reason detail::NormalizeLatitude and
//!   detail::IsDApplicableParams do: a core-internal primitive that is consumed across files but
//!   is not part of the stable surface a caller outside core should bind to.
bool IsFullTurnUniform(DistributionType type, float full_range_deg);

}  // namespace detail

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

//! @brief Index space for the three distributions of a crystal's `axis` object.
//!
//! @details Named after ShapeScalar because it answers the same question in the
//!   same shape — an int slot in, one JSON key out — but the two models are not
//!   parallel and should not be read as such: an axis has no crystal kind to
//!   depend on and therefore no "applicable to this type" concept. Every slot
//!   below always exists. Slot order is the serialization order, nothing more;
//!   unlike ShapeScalar it is NOT an RNG draw order.
//!
//!   Note kAxisScalarZenith names the EXTERNAL wire quantity, which is the
//!   complement of the internal AxisDistribution::latitude_dist it maps onto
//!   (zenith = 90 - latitude). The key names the file format, not the field.
enum AxisScalar : int {
  kAxisScalarZenith = 0,
  kAxisScalarAzimuth = 1,
  kAxisScalarRoll = 2,
  kAxisScalarCount = 3,
};

//! @brief The JSON key under a crystal's `axis` that names axis-scalar `slot`.
//!
//! @details Returns a static string, or nullptr when `slot` is out of range —
//!   matching ShapeScalarSyncKeyName's convention, so a caller iterating a
//!   mismatched slot count degrades to "no such key" instead of an invented one.
//!
//!   These three strings are core's schema. A layer that misspells one silently
//!   drops the field rather than reporting an error, so no layer spells them out.
const char* AxisScalarKeyName(int slot);

//! @brief The "here is how to write this axis slot" guidance appended to every
//!   rejection of a malformed `axis.<slot>` distribution.
//!
//! @details `slot_key` is the bare JSON key ("zenith" / "azimuth" / "roll"), as
//!   returned by AxisScalarKeyName; the returned text spells both legal forms
//!   with that key, so the reader can paste it.
//!
//!   This lives in core, and the C API's own parser calls it, because the two
//!   parsers reject the same documents and their messages are the only migration
//!   guidance an out-of-tree hand-written config will ever get. Two independently
//!   written copies of that guidance would drift, and the drift would be invisible
//!   — nothing compares the two strings. Pure: no throw, no IO.
std::string FormatAxisSlotHint(const std::string& slot_key);

// convertion to & from json object
NLOHMANN_JSON_SERIALIZE_ENUM(  // declear macro
    DistributionType,          // enum type
    {
        { DistributionType::kUniform, "uniform" },
        { DistributionType::kGaussian, "gauss" },
        { DistributionType::kZigzag, "zigzag" },
        { DistributionType::kLaplacian, "laplacian" },
        { DistributionType::kGaussianLegacy, "gauss_legacy" },
    })

void to_json(nlohmann::json& obj, const Distribution& dist);
void from_json(const nlohmann::json& obj, Distribution& dist);

void to_json(nlohmann::json& obj, const AxisDistribution& dist);
void from_json(const nlohmann::json& obj, AxisDistribution& dist);
}  // namespace lumice

#endif  // CORE_RANDOM_H_
