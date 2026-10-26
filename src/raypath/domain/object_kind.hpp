#ifndef LUMICE_RAYPATH_DOMAIN_OBJECT_KIND_HPP_
#define LUMICE_RAYPATH_DOMAIN_OBJECT_KIND_HPP_

#include <string_view>
#include <vector>

namespace lumice::raypath::survey {

// The object-kind registry. Registration order is the document order of any listing of the
// vocabulary; the registry grows append-only (a new kind is a new enum value plus its name —
// never a re-spelling of an old one).
enum class SurveyObjectKind {
  kSourceImage,          // T0 identity certificate (M = I and zero wedge): the sun's preimage
  kFamilyCollapsePoint,  // T1 rank 0 on the limit family
  kRegularImage,         // T1 rank 1: a one-dimensional regular image
  kCausticEdge,          // T1 rank 2: the critical image of the relevant projection
  kBrightDomain,         // a two-dimensional bright domain (weight profiles live here)
  kSupportSet,           // the support set: boundary curves with mechanism labels, holes
};

struct SurveyObjectKindNameEntry {
  SurveyObjectKind kind;
  const char* name;
};

inline const std::vector<SurveyObjectKindNameEntry>& SurveyObjectKindTable() {
  static const std::vector<SurveyObjectKindNameEntry> kTable = {
    { SurveyObjectKind::kSourceImage, "source_image" },
    { SurveyObjectKind::kFamilyCollapsePoint, "family_collapse_point" },
    { SurveyObjectKind::kRegularImage, "regular_image" },
    { SurveyObjectKind::kCausticEdge, "caustic_edge" },
    { SurveyObjectKind::kBrightDomain, "bright_domain" },
    { SurveyObjectKind::kSupportSet, "support_set" },
  };
  return kTable;
}

inline const char* SurveyObjectKindName(SurveyObjectKind kind) {
  for (const auto& entry : SurveyObjectKindTable()) {
    if (entry.kind == kind) {
      return entry.name;
    }
  }
  return "unknown";
}

// False for anything outside the registered vocabulary.
inline bool SurveyObjectKindFromName(std::string_view name, SurveyObjectKind* out) {
  for (const auto& entry : SurveyObjectKindTable()) {
    if (name == entry.name) {
      *out = entry.kind;
      return true;
    }
  }
  return false;
}

// The registry itself, registration order — projected from the name table above, of which it is
// not a second copy. The serializer's coverage row and the assembly's unevaluated entries read
// this rather than keeping their own copies.
inline const std::vector<SurveyObjectKind>& AllSurveyObjectKinds() {
  static const std::vector<SurveyObjectKind> kKinds = [] {
    std::vector<SurveyObjectKind> kinds;
    kinds.reserve(SurveyObjectKindTable().size());
    for (const auto& entry : SurveyObjectKindTable()) {
      kinds.push_back(entry.kind);
    }
    return kinds;
  }();
  return kKinds;
}


}  // namespace lumice::raypath::survey
#endif
