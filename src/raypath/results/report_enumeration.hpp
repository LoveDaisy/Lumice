#ifndef LUMICE_RAYPATH_RESULTS_REPORT_ENUMERATION_HPP_
#define LUMICE_RAYPATH_RESULTS_REPORT_ENUMERATION_HPP_

#include "raypath/results/report_structure.hpp"
#include "raypath/results/structure.hpp"
namespace lumice::raypath::schema3 {

// One declared-not-produced entry: the kind and its reason are a single row, so a reader can
// never walk one column past the other.
struct DeclaredNotProduced {
  ObjectKind kind;
  std::string reason;
};

struct EnumeratedCoverage {
  std::vector<ObjectKind> produced;
  std::vector<DeclaredNotProduced> declared_not_produced;
};

struct EnumerationBudget {
  long long max_optical_evaluations = 0;  // <= 0: unconstrained (666.3 wires the flag)
  long long sampling_evaluations = 0;     // orbit + restricted stream points built
  long long hang_cap_ms = 0;              // the installed anti-hang cap; 0 = none installed
  bool truncated = false;                 // the cap fired somewhere in this enumeration
  std::string truncation_note;            // what was cut (empty when not truncated)
};

struct Schema3DiscoveryCore {
  std::vector<StructureObjectRecord> objects;
  structure::SupportBlock support;
  std::vector<analytic::OrbitFiberStream> orbits;
  EnumeratedCoverage coverage;
  EnumerationBudget budget;
};

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_RESULTS_REPORT_ENUMERATION_HPP_
