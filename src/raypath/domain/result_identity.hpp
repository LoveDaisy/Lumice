#ifndef LUMICE_RAYPATH_DOMAIN_RESULT_IDENTITY_HPP_
#define LUMICE_RAYPATH_DOMAIN_RESULT_IDENTITY_HPP_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "raypath/domain/scope.hpp"

namespace lumice::raypath {

// Identity belongs to a frozen structure, never a container address or a wire ordinal.
class ResultIdentity {
 public:
  ResultIdentity() = default;
  static ResultIdentity Create() {
    static std::atomic<uint64_t> next{ 1 };
    return ResultIdentity(next.fetch_add(1, std::memory_order_relaxed));
  }
  bool operator==(ResultIdentity other) const { return value_ == other.value_; }
  bool operator!=(ResultIdentity other) const { return !(*this == other); }

 private:
  explicit ResultIdentity(uint64_t value) : value_(value) {}
  uint64_t value_ = 0;
};

struct ObjectId {
  size_t ordinal = 0;
};
struct MemberId {
  size_t ordinal = 0;
};
struct ObjectRef {
  ResultIdentity result;
  ObjectId object;
};
struct MemberRef {
  ResultIdentity result;
  MemberId member;
};

// Construction copies/moves into exclusively owned storage before sharing it as const.
// Copies share the same immutable fact mapping. Reassembly always creates a new identity.
template <class Object>
class StructureSnapshot {
 public:
  static std::optional<StructureSnapshot> Finalize(MemberScope members, std::vector<Object> objects,
                                                   std::vector<std::vector<MemberId>> object_members) {
    if (objects.size() != object_members.size()) {
      return std::nullopt;
    }
    for (const auto& row : object_members) {
      for (const MemberId member : row) {
        if (member.ordinal >= members.members.size()) {
          return std::nullopt;
        }
      }
    }
    return StructureSnapshot(std::make_shared<const Storage>(
        Storage{ ResultIdentity::Create(), std::move(members), std::move(objects), std::move(object_members) }));
  }

  ResultIdentity Identity() const { return storage_->identity; }
  const MemberScope& Members() const { return storage_->members; }
  const std::vector<Object>& Objects() const { return storage_->objects; }
  std::optional<ObjectRef> Reference(ObjectId id) const {
    if (id.ordinal >= storage_->objects.size()) {
      return std::nullopt;
    }
    return ObjectRef{ Identity(), id };
  }
  std::optional<MemberRef> Reference(MemberId id) const {
    if (id.ordinal >= storage_->members.members.size()) {
      return std::nullopt;
    }
    return MemberRef{ Identity(), id };
  }
  const Object* Resolve(ObjectRef ref) const {
    return ref.result == Identity() && ref.object.ordinal < storage_->objects.size() ?
               &storage_->objects[ref.object.ordinal] :
               nullptr;
  }
  const std::vector<int>* Resolve(MemberRef ref) const {
    return ref.result == Identity() && ref.member.ordinal < storage_->members.members.size() ?
               &storage_->members.members[ref.member.ordinal] :
               nullptr;
  }
  const std::vector<MemberId>* ObjectMembers(ObjectRef ref) const {
    return Resolve(ref) ? &storage_->object_members[ref.object.ordinal] : nullptr;
  }

 private:
  struct Storage {
    ResultIdentity identity;
    MemberScope members;
    std::vector<Object> objects;
    std::vector<std::vector<MemberId>> object_members;
  };
  explicit StructureSnapshot(std::shared_ptr<const Storage> storage) : storage_(std::move(storage)) {}
  std::shared_ptr<const Storage> storage_;
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DOMAIN_RESULT_IDENTITY_HPP_
