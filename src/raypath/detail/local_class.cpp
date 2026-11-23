#include "raypath/local_class.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "core/math.hpp"
#include "raypath/detail/local_class.hpp"
#include "util/sky_direction.hpp"

namespace lumice::raypath {
namespace {

std::vector<int> ApplyElement(const CrystalElement& element, const std::vector<int>& sequence) {
  std::vector<int> image = sequence;
  for (int& face : image) {
    face = static_cast<int>(element.ApplyToFace(static_cast<IdType>(face)));
  }
  return image;
}

}  // namespace

Error ResolveLocalClasses(const PhysicalMemberScope& row, const LocalClassRequest& request,
                          std::vector<LocalClass>* out) {
  *out = {};
  if (row.snapshot.representative.empty() || row.members.empty()) {
    return {};  // nothing to partition; an empty row is an empty partition
  }
  const auto shape = std::visit([](const auto& p) { return DeriveGeometricSymmetry(p); }, row.snapshot.crystal.param_);
  double target_dir[3];
  AltAzToDir(request.target_altitude_deg, request.target_azimuth_deg, target_dir);
  const double sun_azimuth_rad = request.sun_azimuth_deg * math::kPiD / 180.0;
  std::vector<CrystalElement> sky_fixing;
  for (const CrystalElement& element : DeriveRetainedElements(shape, row.snapshot.crystal.axis_)) {
    if (ElementFixesSkyDirection(element, target_dir, sun_azimuth_rad)) {
      sky_fixing.push_back(element);
    }
  }

  // Distinct row members, first-occurrence index kept (the scope already dedups; defensive
  // against a duplicated member list so a class never counts one member twice).
  std::map<std::vector<int>, size_t> first_index;
  for (size_t i = 0; i < row.members.size(); ++i) {
    first_index.emplace(row.members[i], i);
  }

  // Partition by BFS over the sky-fixing elements, clamped to the row: an image that leaves the
  // row is not a member (a partial-bit row keeps its own scope). Compositions of sky-fixing
  // retained elements are themselves enumerated members of the D6h inventory, so single-step
  // closure over `sky_fixing` reaches the whole class.
  std::vector<bool> visited(row.members.size(), false);
  std::vector<LocalClass> classes;
  for (size_t seed = 0; seed < row.members.size(); ++seed) {
    if (visited[seed]) {
      continue;
    }
    LocalClass klass;
    std::vector<size_t> queue{ seed };
    visited[seed] = true;
    std::set<std::vector<int>> sequences{ row.members[seed] };
    while (!queue.empty()) {
      const size_t index = queue.back();
      queue.pop_back();
      klass.member_indices.push_back(index);
      for (const CrystalElement& element : sky_fixing) {
        const std::vector<int> image = ApplyElement(element, row.members[index]);
        const auto found = first_index.find(image);
        if (found == first_index.end()) {
          continue;  // leaves the row: not a member here
        }
        if (sequences.insert(image).second) {
          visited[found->second] = true;
          queue.push_back(found->second);
        }
      }
    }
    klass.representative = *sequences.begin();
    std::sort(klass.member_indices.begin(), klass.member_indices.end());
    // The transport facts index the SAME order as member_indices (sorted, ascending).
    klass.to_representative.reserve(klass.member_indices.size());
    for (size_t index : klass.member_indices) {
      CrystalElement carrier = CrystalElement{};  // the identity carries the representative
      for (const CrystalElement& element : sky_fixing) {
        if (ApplyElement(element, row.members[index]) == klass.representative) {
          carrier = element;
          break;  // first in the inventory's fixed order: deterministic
        }
      }
      klass.to_representative.push_back(carrier);
      klass.member_sequences.push_back(row.members[index]);
    }
    classes.push_back(std::move(klass));
  }
  std::sort(classes.begin(), classes.end(),
            [](const LocalClass& a, const LocalClass& b) { return a.representative < b.representative; });
  *out = std::move(classes);
  return {};
}
}  // namespace lumice::raypath
