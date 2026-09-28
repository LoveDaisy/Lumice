#include "core/device_filter_desc.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <variant>
#include <vector>

#include "config/filter_config.hpp"
#include "core/crystal.hpp"
#include "core/def.hpp"
#include "core/math.hpp"

namespace lumice {
namespace detail {

namespace {

// Mirror of `RaypathOrbit`/`BuildOrbit` (filter_spec.cpp): the canonical hits of the configured
// raypath. We recompute here rather than calling `BuildOrbit` to keep `filter_spec.cpp`
// platform-agnostic (plan D5).
//
// Reduced under the descriptor's own gating fields — the same request ∩ ensemble ∩ shape the
// device reduction (ReduceBuffer_dev) reads per ray — so the canonical form and the reduced ray
// always meet, and both equal what FilterSpec computes on the CPU path.
void FillCanonicalBytes(const Crystal& crystal, const std::vector<IdType>& rp, DeviceFilterDesc& out) {
  const GeometricSymmetry geom{ out.shape_p_step, out.shape_d_mirror_mask, out.shape_b_applicable != 0u };
  auto canonical = ReduceRaypathByPeriod(rp, out.symmetry, out.sigma_a, out.d_applicable != 0u, out.p_applicable != 0u,
                                         out.b_applicable != 0u, crystal.FnPeriod(), geom);
  out.canonical_len = static_cast<uint8_t>(std::min<size_t>(canonical.size(), kMaxHits));
  for (uint8_t i = 0; i < out.canonical_len; ++i) {
    out.canonical_bytes[i] = static_cast<uint8_t>(canonical[i] & 0xFF);
  }
}

void FillRaypath(const Crystal& crystal, const RaypathFilterParam& p, DeviceFilterDesc& out) {
  out.type = kDeviceFilterTypeRaypath;
  FillCanonicalBytes(crystal, p.raypath_, out);
}

void FillEntryExit(const Crystal& crystal, const EntryExitFilterParam& p, DeviceFilterDesc& out) {
  out.type = kDeviceFilterTypeEntryExit;
  out.has_entry = p.entry_.has_value() ? 1u : 0u;
  out.has_exit = p.exit_.has_value() ? 1u : 0u;
  out.min_len = static_cast<uint32_t>(p.min_len_);
  // max_len == 0 sentinels "no upper bound" on device (matches plan D2).
  out.max_len = p.max_len_.has_value() ? static_cast<uint32_t>(*p.max_len_) : 0u;

  // BuildOrbitFromEnds: empty rp ⇒ length-only match (has_orbit_ guards Match
  // on host; the device path mirrors via has_entry==0 && has_exit==0).
  std::vector<IdType> rp;
  if (p.entry_.has_value()) {
    rp.push_back(*p.entry_);
  }
  if (p.exit_.has_value()) {
    rp.push_back(*p.exit_);
  }
  if (!rp.empty()) {
    FillCanonicalBytes(crystal, rp, out);
  }
}

void FillDirection(const DirectionFilterParam& p, DeviceFilterDesc& out) {
  // Mirror of DirectionSpec ctor (filter_spec.cpp:243-247).
  out.type = kDeviceFilterTypeDirection;
  float lon_rad = p.lon_ * math::kDegreeToRad;
  float lat_rad = p.lat_ * math::kDegreeToRad;
  out.dir[0] = std::cos(lat_rad) * std::cos(lon_rad);
  out.dir[1] = std::cos(lat_rad) * std::sin(lon_rad);
  out.dir[2] = std::sin(lat_rad);
  out.radii_c = std::cos(p.radii_ * math::kDegreeToRad);
}

void FillCrystal(const CrystalFilterParam& p, DeviceFilterDesc& out) {
  out.type = kDeviceFilterTypeCrystal;
  out.crystal_id = static_cast<uint32_t>(p.crystal_id_);
}

// `out` already carries symmetry / sigma_a and the six gating fields; the Fill* helpers read them.
struct SimpleVisitor {
  const Crystal& crystal;
  DeviceFilterDesc& out;

  void operator()(const NoneFilterParam& /*p*/) const { out.type = kDeviceFilterTypeNone; }
  void operator()(const RaypathFilterParam& p) const { FillRaypath(crystal, p, out); }
  void operator()(const EntryExitFilterParam& p) const { FillEntryExit(crystal, p, out); }
  void operator()(const DirectionFilterParam& p) const { FillDirection(p, out); }
  void operator()(const CrystalFilterParam& p) const { FillCrystal(p, out); }
};

// Fill the Complex filter's top-level fields (type / or_clause_count). Does
// NOT touch sub_desc_start / and_terms_start — those fields are owned by the
// caller (`EnsureFilterBuffers`) which assigns them before appending the
// sub-descs and AND-term counts via `BuildComplexSubDescs`.
//
// task-device-flat-and-terms: the former inline `and_term_counts[8]` array
// has been removed; per-OR-clause AND-term counts now live in a separate
// host-built flat buffer that `BuildComplexSubDescs` appends into.
void FillComplexDescTop(const ComplexFilterParam& p, DeviceFilterDesc& out) {
  assert(p.filters_.size() <= kDeviceFilterOrClauseSanityCap &&
         "Complex filter exceeds kDeviceFilterOrClauseSanityCap sanity bound; check host ABI clamp");
  out.type = kDeviceFilterTypeComplex;
  out.or_clause_count = static_cast<uint16_t>(p.filters_.size());
}

struct TopVisitor {
  const Crystal& crystal;
  DeviceFilterDesc& out;

  void operator()(const SimpleFilterParam& p) const { std::visit(SimpleVisitor{ crystal, out }, p); }
  void operator()(const ComplexFilterParam& p) const {
    // Top-level desc only: or_clause_count + and_term_counts. sub_desc_start
    // is assigned in EnsureFilterBuffers immediately before BuildComplexSubDescs
    // is invoked, so it stays 0 here.
    FillComplexDescTop(p, out);
  }
};

}  // namespace

DeviceFilterDesc BuildDeviceFilterDesc(const FilterConfig& config, const Crystal& crystal,
                                       const AxisDistribution& axis_dist) {
  DeviceFilterDesc desc{};  // zero-init all fields
  desc.action = (config.action_ == FilterConfig::kFilterIn) ? 0u : 1u;
  desc.symmetry = config.symmetry_;
  bool d_applicable = detail::IsDApplicable(axis_dist);
  desc.d_applicable = d_applicable ? 1u : 0u;
  desc.sigma_a = d_applicable ? detail::ComputeSigmaA(axis_dist.roll_dist.center) : 0;
  desc.fn_period = crystal.FnPeriod();
  // The one place the gating fields are computed; sub-descs and the canonical bytes copy them.
  // A filter's P/B/D is a label equivalence (SymmetrySemantics::kLabel), exactly as on the CPU
  // path (FilterSpec::Create): the shape half is the full D6h and P/B are admitted whatever the
  // real shape and ensemble. D's ensemble half (d_applicable / sigma_a above) is not part of it.
  const SymmetryGating gating = DeriveSymmetryGating(SymmetrySemantics::kLabel, crystal.GeomSymmetry(), axis_dist);
  desc.p_applicable = gating.p_applicable ? 1u : 0u;
  desc.b_applicable = gating.b_applicable ? 1u : 0u;
  desc.shape_p_step = gating.geom.p_step;
  desc.shape_d_mirror_mask = gating.geom.d_valid_sigma_mask;
  desc.shape_b_applicable = gating.geom.b_applicable ? 1u : 0u;

  std::visit(TopVisitor{ crystal, desc }, config.param_);
  return desc;
}

void BuildComplexSubDescs(const ComplexFilterParam& p, const Crystal& crystal, const DeviceFilterDesc& parent,
                          std::vector<DeviceFilterDesc>& out_sub_descs, std::vector<uint8_t>& out_and_term_counts) {
  assert(p.filters_.size() <= kDeviceFilterOrClauseSanityCap &&
         "Complex filter exceeds kDeviceFilterOrClauseSanityCap sanity bound; check host ABI clamp");
  for (const auto& or_clause : p.filters_) {
    // Same uint8_t bound as before; kept here so an unsynced caller trips it
    // even if the top-level desc builder is bypassed.
    assert(or_clause.size() <= 255u && "Complex AND-term count exceeds uint8_t (255)");
    // task-device-flat-and-terms: same pass as sub-desc collection appends the
    // per-OR-clause AND-term count into the parallel flat buffer, avoiding a
    // second traversal.
    out_and_term_counts.push_back(static_cast<uint8_t>(or_clause.size()));
    for (const auto& and_entry : or_clause) {
      // Mirror ComplexSpec ctor (filter_spec.cpp): sub-spec inherits the parent
      // Complex's symmetry / sigma_a and every symmetry-gating field.
      // and_entry.first is the host-side filter id (unused on device —
      // sub_specs are inlined by position).
      DeviceFilterDesc sub{};
      sub.symmetry = parent.symmetry;
      sub.d_applicable = parent.d_applicable;
      sub.sigma_a = parent.sigma_a;
      sub.fn_period = parent.fn_period;
      sub.p_applicable = parent.p_applicable;
      sub.b_applicable = parent.b_applicable;
      sub.shape_p_step = parent.shape_p_step;
      sub.shape_d_mirror_mask = parent.shape_d_mirror_mask;
      sub.shape_b_applicable = parent.shape_b_applicable;
      // sub-filter predicate only; the top-level Complex `action` is XORed by
      // DeviceFilterCheck. Mirrors host ComplexSpec::Match calling
      // `and_f->Match` (not `Check`) per filter_spec.cpp.
      sub.action = 0u;
      std::visit(SimpleVisitor{ crystal, sub }, and_entry.second);
      out_sub_descs.push_back(sub);
    }
  }
}

std::vector<uint8_t> BuildDeviceGetFnBytes(const Crystal& crystal) {
  size_t n = crystal.PolygonFaceCount();
  std::vector<uint8_t> bytes(n, 0u);
  for (size_t i = 0; i < n; ++i) {
    IdType fn = crystal.GetFn(static_cast<IdType>(i));
    // GetFn returns kInvalidId for out-of-range; that only happens on a
    // mismatched crystal/poly index, which is a programming error elsewhere.
    // We clamp to 0xFF to keep the byte stream well-defined.
    bytes[i] = static_cast<uint8_t>(fn & 0xFF);
  }
  return bytes;
}

}  // namespace detail
}  // namespace lumice
