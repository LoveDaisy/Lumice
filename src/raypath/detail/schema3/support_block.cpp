#include "raypath/detail/schema3/support_block.hpp"

#include <cmath>
#include <utility>

#include "analytic/dp_weight_kink.hpp"

namespace lumice::raypath::schema3 {
namespace {

// The partition's own merge constant is the "same value" ruler everywhere on this block: an
// onset sits AT an interval endpoint, and a kink circle is CONSTANT along itself, within it.
constexpr double kSameValueTol = analytic::kExtremumAtol;

bool AtEndpoint(double value, const std::vector<analytic::DeviationInterval>& intervals) {
  for (const analytic::DeviationInterval& interval : intervals) {
    if (std::fabs(value - interval.lower) <= kSameValueTol || std::fabs(value - interval.upper) <= kSameValueTol) {
      return true;
    }
  }
  return false;
}

// The closed-form constant-D_P circles at the base index, then the n-continuation leg: the same
// weight step re-read at each of the caller's indices (one WeightKinks pass per index, shared by
// every curve — the marched steps of that pass are the price of the authority).
std::vector<ConstantDeltaCurve> ConstantCurvesFor(const analytic::FaceNormalTable& normals,
                                                  const analytic::FacePolygonTable& polygons, const int* slots,
                                                  int slot_count, double base_index,
                                                  const std::vector<double>& wavelengths_nm,
                                                  const std::vector<double>& indices) {
  const analytic::DeviationField base(normals, polygons, slots, slot_count, base_index);
  const std::vector<analytic::KinkCurve> base_kinks = analytic::WeightKinks(base, analytic::KinkOptions{});
  std::vector<std::vector<analytic::KinkCurve>> per_lambda_kinks;
  per_lambda_kinks.reserve(std::min(wavelengths_nm.size(), indices.size()));
  for (size_t k = 0; k < indices.size() && k < wavelengths_nm.size(); k++) {
    const analytic::DeviationField per_lambda(normals, polygons, slots, slot_count, indices[k]);
    per_lambda_kinks.push_back(analytic::WeightKinks(per_lambda, analytic::KinkOptions{}));
  }
  std::vector<ConstantDeltaCurve> out;
  for (const analytic::KinkCurve& kink : base_kinks) {
    if (kink.coverage != analytic::KinkCoverage::kClosedFormAuthority || kink.arcs.empty() ||
        kink.arcs[0].values.empty()) {
      continue;
    }
    if (!(std::fabs(kink.Spread()) <= kSameValueTol)) {
      continue;  // a curve whose D_P moves along itself is an object for the enumeration, not a
                 // single delta-axis fact (a NaN spread without arcs never passes the test)
    }
    ConstantDeltaCurve curve;
    curve.d_p = kink.arcs[0].values[0];
    curve.weight_step = kink.step;
    curve.wavelengths_nm = wavelengths_nm;
    curve.critical_d_p.assign(wavelengths_nm.size(), std::nan(""));  // NaN = not found there
    for (size_t k = 0; k < per_lambda_kinks.size() && k < curve.critical_d_p.size(); k++) {
      for (const analytic::KinkCurve& per_kink : per_lambda_kinks[k]) {
        if (per_kink.step == kink.step && per_kink.coverage == analytic::KinkCoverage::kClosedFormAuthority &&
            !per_kink.arcs.empty() && !per_kink.arcs[0].values.empty() &&
            std::fabs(per_kink.Spread()) <= kSameValueTol) {
          curve.critical_d_p[k] = per_kink.arcs[0].values[0];
          break;
        }
      }
    }
    out.push_back(std::move(curve));
  }
  return out;
}

MemberSupport RowFrom(const AxisAssembly& assembly, const analytic::FaceNormalTable& normals,
                      const analytic::FacePolygonTable& polygons, const int* slots, int slot_count, double base_index,
                      const std::vector<int>& member, const std::vector<double>& wavelengths_nm,
                      const std::vector<double>& indices) {
  MemberSupport row;
  row.member = member;
  row.axis = assembly.axis;
  if (!assembly.axis.walk_closed || assembly.axis.context.coverage != PartitionContext::Coverage::kComplete) {
    // Fail closed: a refused or escaped axis has no intervals, hence no endpoint objects and no
    // constant-curve reading — the refusal is the row's content.
    return row;
  }
  for (const analytic::CriticalOnset& onset : assembly.onsets) {
    if (AtEndpoint(onset.value, assembly.axis.intervals)) {
      row.endpoint_onsets.push_back(onset);
    }
  }
  row.constant_curves = ConstantCurvesFor(normals, polygons, slots, slot_count, base_index, wavelengths_nm, indices);
  return row;
}

}  // namespace

MemberSupport SupportRowOf(const AxisAssembly& assembly, const analytic::FaceNormalTable& normals,
                           const analytic::FacePolygonTable& polygons, const int* slots, int slot_count,
                           double base_index, const std::vector<int>& member, const std::vector<double>& wavelengths_nm,
                           const std::vector<double>& indices) {
  return RowFrom(assembly, normals, polygons, slots, slot_count, base_index, member, wavelengths_nm, indices);
}

MemberSupport MemberSupportOf(const analytic::FaceNormalTable& normals, const analytic::FacePolygonTable& polygons,
                              const int* slots, int slot_count, double base_index, const std::vector<int>& member,
                              const std::vector<double>& wavelengths_nm, const std::vector<double>& indices) {
  const analytic::DeviationField field(normals, polygons, slots, slot_count, base_index);
  return RowFrom(AssembleAxis(field), normals, polygons, slots, slot_count, base_index, member, wavelengths_nm,
                 indices);
}

FamilySupport AggregateFamily(const std::vector<MemberSupport>& rows) {
  FamilySupport out;
  if (rows.empty()) {
    return out;
  }
  for (const MemberSupport& row : rows) {
    if (row.axis.context.coverage != PartitionContext::Coverage::kComplete) {
      return out;  // one refusal and the family shares nothing
    }
  }
  const std::vector<analytic::DeviationInterval>& first = rows[0].axis.intervals;
  for (const MemberSupport& row : rows) {
    if (!SameSupport(row.axis.intervals, first)) {
      return out;
    }
  }
  out.shared = true;
  out.intervals = first;
  return out;
}

bool SameSupport(const std::vector<analytic::DeviationInterval>& a, const std::vector<analytic::DeviationInterval>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); i++) {
    if (std::fabs(a[i].lower - b[i].lower) > kSameValueTol || std::fabs(a[i].upper - b[i].upper) > kSameValueTol) {
      return false;
    }
  }
  return true;
}

}  // namespace lumice::raypath::schema3
