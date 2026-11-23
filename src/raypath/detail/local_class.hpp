#ifndef LUMICE_RAYPATH_DETAIL_LOCAL_CLASS_HPP_
#define LUMICE_RAYPATH_DETAIL_LOCAL_CLASS_HPP_

#include "raypath/detail/physical_member_scope.hpp"
#include "raypath/local_class.hpp"

namespace lumice::raypath {

// Resolves the partition. Errors only on a scope that cannot be interpreted at all (a scope
// whose crystal variant carries no shape); an empty row member list resolves to an empty
// partition.
Error ResolveLocalClasses(const PhysicalMemberScope& row, const LocalClassRequest& request,
                          std::vector<LocalClass>* out);

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_DETAIL_LOCAL_CLASS_HPP_
