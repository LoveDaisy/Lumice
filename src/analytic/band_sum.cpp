#include "analytic/band_sum.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "analytic/path_rank.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

double Clamp1(double x) {
  return x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x);
}

double Deviation(const double x[3], const double s[3]) {
  return std::acos(Clamp1(so3::Dot3(x, s)));
}

void Normalize3(double v[3]) {
  const double n = so3::Norm3(v);
  for (int i = 0; i < 3; i++) {
    v[i] /= n;
  }
}

// The event half of the pose's third row (section 4.3): rows j of F_i = [u, f, u x f], f =
// unit(phi + cos(D) u), so that (R_i)_3j = W_3 . rows[j].
struct EventRows {
  double rows[3][3];
};

EventRows RowsOf(const BandEvent& e) {
  const double cos_d = std::cos(e.deviation);
  double f[3] = { e.phi[0] + cos_d * e.u[0], e.phi[1] + cos_d * e.u[1], e.phi[2] + cos_d * e.u[2] };
  Normalize3(f);
  double uf[3];
  so3::Cross3(e.u, f, uf);
  EventRows r;
  for (int j = 0; j < 3; j++) {
    r.rows[j][0] = e.u[j];
    r.rows[j][1] = f[j];
    r.rows[j][2] = uf[j];
  }
  return r;
}

// The pixel half: the third row of W = [s_hat, e, s_hat x e], e = unit(d - (d . s_hat) s_hat).
void PixelRow(const double sun[3], const double centre[3], double w3[3]) {
  const double ds = so3::Dot3(centre, sun);
  double e[3] = { centre[0] - ds * sun[0], centre[1] - ds * sun[1], centre[2] - ds * sun[2] };
  Normalize3(e);
  double se[3];
  so3::Cross3(sun, e, se);
  w3[0] = sun[2];
  w3[1] = e[2];
  w3[2] = se[2];
}

// ---- the rank-0 psi average (section 5, "a deterministic alternative") ----

// 16-point Gauss-Legendre panels; the composite rule doubles its panel count until the mass settles.
constexpr int kPsiPanelNodes = 16;
constexpr int kPsiInitialPanels = 4;
constexpr int kPsiMaximumPanels = 1024;
constexpr double kPsiRelativeChange = 1e-6;

struct PanelRule {
  double x[kPsiPanelNodes];
  double w[kPsiPanelNodes];
  PanelRule() { GaussLegendre(kPsiPanelNodes, x, w); }
};

// The twist frame of the sun (independent of the event): e' a unit vector normal to s_hat and
// (s_hat x e'); p, q their zenith components.
struct SunFrame {
  double sz = 0.0;
  double p = 0.0;
  double q = 0.0;
};

SunFrame MakeSunFrame(const double sun[3]) {
  const double z[3] = { 0.0, 0.0, 1.0 };
  double e[3];
  so3::Cross3(z, sun, e);
  if (so3::Norm3(e) < 1e-12) {
    e[0] = 1.0;
    e[1] = 0.0;
    e[2] = 0.0;
  }
  Normalize3(e);
  double se[3];
  so3::Cross3(sun, e, se);
  return { sun[2], e[2], se[2] };
}

// The third row of R_psi = Rot(s_hat, psi) R_0, R_0 u = s_hat, as e_j(psi) = c_j + a_j cos psi +
// b_j sin psi: R_0 b_j = u_j s_hat + f_j e' + g_j (s_hat x e') for any unit f normal to u and g = u
// x f, and the twist about s_hat keeps the s_hat part and turns the rest.
struct TwistRow {
  double c[3];
  double a[3];
  double b[3];
};

TwistRow MakeTwistRow(const double u[3], const SunFrame& sun) {
  // f: the axis least aligned with u, made normal to it.
  int k = 0;
  for (int i = 1; i < 3; i++) {
    if (std::fabs(u[i]) < std::fabs(u[k])) {
      k = i;
    }
  }
  double f[3] = { 0.0, 0.0, 0.0 };
  f[k] = 1.0;
  const double fu = so3::Dot3(f, u);
  for (int i = 0; i < 3; i++) {
    f[i] -= fu * u[i];
  }
  Normalize3(f);
  double g[3];
  so3::Cross3(u, f, g);
  TwistRow t;
  for (int j = 0; j < 3; j++) {
    t.c[j] = u[j] * sun.sz;
    t.a[j] = f[j] * sun.p + g[j] * sun.q;
    t.b[j] = f[j] * sun.q - g[j] * sun.p;
  }
  return t;
}

double RhoAt(const TwistRow& t, const PoseDensity& density, double psi) {
  const double c = std::cos(psi);
  const double s = std::sin(psi);
  return density.Evaluate(t.c[0] + t.a[0] * c + t.b[0] * s, t.c[1] + t.a[1] * c + t.b[1] * s,
                          t.c[2] + t.a[2] * c + t.b[2] * s);
}

double Composite(const PanelRule& rule, int panels, double lo, double hi, const TwistRow& t,
                 const PoseDensity& density) {
  const double width = (hi - lo) / panels;
  double sum = 0.0;
  for (int p = 0; p < panels; p++) {
    const double mid = lo + (p + 0.5) * width;
    for (int i = 0; i < kPsiPanelNodes; i++) {
      sum += rule.w[i] * RhoAt(t, density, mid + 0.5 * width * rule.x[i]);
    }
  }
  return 0.5 * width * sum;
}

// <rho>_psi(u) = (1 / 2 pi) oint rho(R_psi) dpsi, integrated only where the c-axis zenith can reach
// the density's window: e3(psi) = c_3 + A cos(psi - phi_3) sweeps [c_3 - A, c_3 + A], and the psi
// with cos(theta) inside the window's [cos theta_hi, cos theta_lo] are phi_3 +- [t1, t2].
double PsiAverage(const TwistRow& t, const PoseDensity& density, const PanelRule& rule, int panels) {
  double theta_lo = 0.0;
  double theta_hi = 0.0;
  density.ZenithWindow(&theta_lo, &theta_hi);
  const double amplitude = std::hypot(t.a[2], t.b[2]);
  if (amplitude < 1e-12) {
    return Composite(rule, panels, -kPi, kPi, t, density) / (2.0 * kPi);
  }
  const double phase = std::atan2(t.b[2], t.a[2]);
  const double high = std::fmin(1.0, (std::cos(theta_lo) - t.c[2]) / amplitude);
  const double low = std::fmax(-1.0, (std::cos(theta_hi) - t.c[2]) / amplitude);
  if (!(low <= high)) {
    return 0.0;  // the zenith never enters the window: rho < exp(-72) of its peak at every twist
  }
  const double t1 = std::acos(high);
  const double t2 = std::acos(low);
  return (Composite(rule, panels, phase + t1, phase + t2, t, density) +
          Composite(rule, panels, phase - t2, phase - t1, t, density)) /
         (2.0 * kPi);
}

}  // namespace

bool PixelContains(const double corners[12], const double x[3]) {
  double sum[3] = { 0.0, 0.0, 0.0 };
  for (int k = 0; k < 4; k++) {
    for (int i = 0; i < 3; i++) {
      sum[i] += corners[3 * k + i];
    }
  }
  if (!(so3::Dot3(x, sum) > 0.0)) {
    return false;
  }
  bool positive = false;
  bool negative = false;
  for (int k = 0; k < 4; k++) {
    const double* a = corners + 3 * k;
    const double* b = corners + 3 * ((k + 1) % 4);
    double ab[3];
    so3::Cross3(a, b, ab);
    const double det = so3::Dot3(ab, x);
    positive = positive || det > 0.0;
    negative = negative || det < 0.0;
  }
  return !(positive && negative);
}

void BandSumOnEvents(const std::vector<SampleEvent>& events, int n, const double incident[3], const PixelTable& pixels,
                     const PoseDensity& density, std::vector<PixelValue>* out) {
  const double sun[3] = { -incident[0], -incident[1], -incident[2] };
  std::vector<double> deviations(events.size());
  for (size_t i = 0; i < events.size(); i++) {
    deviations[i] = events[i].event.deviation;
  }
  std::vector<EventRows> rows;
  if (!density.IsUniform()) {
    rows.reserve(events.size());
    for (const SampleEvent& e : events) {
      rows.push_back(RowsOf(e.event));
    }
  }
  for (int p = 0; p < pixels.count; p++) {
    const double* centre = pixels.centre + 3 * static_cast<size_t>(p);
    const double* corners = pixels.corners + 12 * static_cast<size_t>(p);
    PixelValue v;
    v.delta = Deviation(centre, incident);
    v.delta_lo = std::numeric_limits<double>::infinity();
    v.delta_hi = -std::numeric_limits<double>::infinity();
    for (int k = 0; k < 4; k++) {
      const double d = Deviation(corners + 3 * k, incident);
      v.delta_lo = std::fmin(v.delta_lo, d);
      v.delta_hi = std::fmax(v.delta_hi, d);
    }
    if (PixelContains(corners, incident) || PixelContains(corners, sun)) {
      v.status = PixelStatus::kSingular;
      v.value = kNaN;
      v.k_eff = kNaN;
      out->push_back(v);
      continue;
    }
    // [delta_lo, delta_hi): searchsorted left at both ends.
    const size_t begin =
        static_cast<size_t>(std::lower_bound(deviations.begin(), deviations.end(), v.delta_lo) - deviations.begin());
    const size_t end =
        static_cast<size_t>(std::lower_bound(deviations.begin(), deviations.end(), v.delta_hi) - deviations.begin());
    double w3[3] = { 0.0, 0.0, 0.0 };
    if (!density.IsUniform() && begin < end) {
      PixelRow(sun, centre, w3);
    }
    double sum = 0.0;
    double sum_sq = 0.0;
    int positive = 0;
    for (size_t i = begin; i < end; i++) {
      double rho = 1.0;
      if (!density.IsUniform()) {
        const EventRows& r = rows[i];
        rho = density.Evaluate(so3::Dot3(w3, r.rows[0]), so3::Dot3(w3, r.rows[1]), so3::Dot3(w3, r.rows[2]));
      }
      const double c = events[i].weight * rho;
      sum += c;
      sum_sq += c * c;
      positive += c > 0.0 ? 1 : 0;
    }
    v.k = static_cast<int>(end - begin);
    v.k_rho_pos = positive;
    v.k_eff = sum_sq > 0.0 ? sum * sum / sum_sq : 0.0;
    const double band = v.delta_hi - v.delta_lo;
    v.value = sum != 0.0 ? sum / (2.0 * kPi * static_cast<double>(n) * band * std::sin(v.delta)) : 0.0;
    out->push_back(v);
  }
}

PointMass RankZeroMass(const std::vector<SampleEvent>& events, int n, const double incident[3],
                       const PoseDensity& density) {
  PointMass out;
  if (density.IsUniform()) {
    double sum = 0.0;
    for (const SampleEvent& e : events) {
      sum += e.weight;
    }
    out.m = sum / static_cast<double>(n);
    out.method = PointMassMethod::kLatticeMean;
    return out;
  }
  out.method = PointMassMethod::kPsiAverage;
  const double sun[3] = { -incident[0], -incident[1], -incident[2] };
  const SunFrame frame = MakeSunFrame(sun);
  std::vector<TwistRow> twists;
  twists.reserve(events.size());
  for (const SampleEvent& e : events) {
    twists.push_back(MakeTwistRow(e.event.u, frame));
  }
  static const PanelRule rule;
  auto mass = [&](int panels) {
    double sum = 0.0;
    for (size_t i = 0; i < events.size(); i++) {
      sum += events[i].weight * PsiAverage(twists[i], density, rule, panels);
    }
    return sum / static_cast<double>(n);
  };
  double previous = mass(kPsiInitialPanels);
  for (int panels = 2 * kPsiInitialPanels; panels <= kPsiMaximumPanels; panels *= 2) {
    const double current = mass(panels);
    out.m = current;
    out.error = std::fabs(current - previous);
    if (out.error <= kPsiRelativeChange * std::fabs(current)) {
      break;
    }
    previous = current;
  }
  return out;
}

void PlacePointMass(double m, const double incident[3], const PixelTable& pixels, std::vector<PixelValue>* out,
                    int* point_mass_pixel) {
  *point_mass_pixel = -1;
  for (int p = 0; p < pixels.count; p++) {
    PixelValue v;
    v.delta = kNaN;
    v.delta_lo = kNaN;
    v.delta_hi = kNaN;
    v.k_eff = kNaN;
    v.value = 0.0;
    if (*point_mass_pixel < 0 && PixelContains(pixels.corners + 12 * static_cast<size_t>(p), incident)) {
      *point_mass_pixel = p;
      v.status = PixelStatus::kPointMass;
      v.value = m / pixels.solid_angle[p];
    }
    out->push_back(v);
  }
}

BandSumOutput BandSum(const FaceNormalTable& table, const FacePolygonTable& polygons, const int* slots, int slot_count,
                      double refractive_index, const double incident[3], int n, const PixelTable& pixels,
                      const PoseDensity& density) {
  BandSumOutput out;
  IceDiscovery sampler(table, polygons, slots, slot_count, refractive_index, incident);
  const std::vector<SampleEvent> events = sampler.BuildEvents(n);
  out.kept_count = static_cast<int>(events.size());
  out.pixels.reserve(static_cast<size_t>(pixels.count));
  out.rank_zero = IsRankZeroPath(table, slots, slot_count);
  if (out.rank_zero) {
    const PointMass mass = RankZeroMass(events, n, incident, density);
    out.m = mass.m;
    out.m_error = mass.error;
    out.method = mass.method;
    PlacePointMass(out.m, incident, pixels, &out.pixels, &out.point_mass_pixel);
    return out;
  }
  BandSumOnEvents(events, n, incident, pixels, density, &out.pixels);
  return out;
}

}  // namespace lumice::analytic
