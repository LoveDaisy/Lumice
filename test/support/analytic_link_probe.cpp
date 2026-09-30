// Link probe for liblumice_analytic's closure: an executable made of exactly the objects that
// library is made of — lumice_foundation_obj, lumice_analytic_kernel and analytic_api.cpp — and
// nothing else of the engine. See the CMake target lumice_analytic_link_probe for why an
// executable and not the library itself: it links without dead-code stripping and cannot leave a
// symbol unresolved, so any reference from any of those objects to a layer above them fails here.
#include "lumice_analytic.h"

int main() {
  return LUMICE_ANALYTIC_GetApiVersion() == LUMICE_ANALYTIC_API_VERSION ? 0 : 1;
}
