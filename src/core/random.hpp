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

#include "core/distribution.hpp"

namespace lumice {

struct AxisDistribution;

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

//! @brief The CPU random engine: a std::mt19937 core behind two standard-library distribution
//!   adapters.
//! @details Reproducibility contract. The mt19937 core stream is fully specified by the standard,
//!   but the adapters wrapped over it — gauss_dist_ (std::normal_distribution<float>) and
//!   uniform_dist_ (std::uniform_real_distribution<float>), consumed by GetGaussian() /
//!   GetUniform() — specify only the output *law*, not the algorithm, and each standard library
//!   fills that freedom with its own transform: the same core stream draws different float
//!   sequences under libc++, libstdc++ and MSVC STL. GPU backends run their own device-side
//!   generators, and the ISA level can change float rounding along the trace path. Bit-exact
//!   replay of a seeded run therefore holds only within one platform + one standard library +
//!   one ISA level + one trace backend; across any of those axes the same seed yields
//!   statistically equivalent results (the Monte Carlo semantics), never a bit-identical image.
//!   The user-facing statement of this contract lives in doc/configuration.md,
//!   "Seed (`--seed`) and Reproducibility".
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
