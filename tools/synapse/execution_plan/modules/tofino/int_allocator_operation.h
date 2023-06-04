#pragma once

#include "tofino_module.h"
#include "ignore.h"

namespace synapse {
namespace targets {
namespace tofino {

class IntegerAllocatorOperation : public TofinoModule {
protected:
  IntegerAllocatorRef int_allocator;

public:
  IntegerAllocatorOperation(ModuleType _type, const char *_name)
      : TofinoModule(_type, _name) {}

  IntegerAllocatorOperation(ModuleType _type, const char *_name,
                            BDD::Node_ptr node,
                            IntegerAllocatorRef _int_allocator)
      : TofinoModule(_type, _name, node), int_allocator(_int_allocator) {}

protected:
  bool can_place(const ExecutionPlan &ep, addr_t obj,
                 IntegerAllocatorRef &implementation) const {
    auto mb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);

    auto possible = mb->check_implementation_compatibility(
        obj, {
                 DataStructure::Type::INTEGER_ALLOCATOR,
                 DataStructure::Type::TABLE,
             });

    if (!possible) {
      return false;
    }

    auto impls = mb->get_implementations(obj);
    auto found = false;

    for (auto impl : impls) {
      if (impl->get_type() == DataStructure::INTEGER_ALLOCATOR) {
        assert(!found && "Only one integer allocator should be found.");
        implementation = std::dynamic_pointer_cast<IntegerAllocator>(impl);
        found = true;
      }

      else if (impl->get_type() == DataStructure::TABLE) {
        auto table = static_cast<Table *>(impl.get());

        // If the table is already managing expirations, we shouldn't be using
        // an Integer Allocator.
        if (table->is_managing_expirations()) {
          return false;
        }
      }
    }

    return true;
  }

  void save_decision(const ExecutionPlan &ep,
                     DataStructureRef int_allocator) const {
    auto objs = int_allocator->get_objs();
    assert(objs.size() == 1);
    auto obj = *objs.begin();

    auto mb = ep.get_memory_bank();
    mb->save_placement_decision(obj, PlacementDecision::IntegerAllocator);

    auto tmb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);
    tmb->save_implementation(int_allocator);
  }

  bool operations_already_done(const ExecutionPlan &ep,
                               IntegerAllocatorRef target,
                               const std::vector<ModuleType> &types) const {
    if (!target) {
      return false;
    }

    auto prev_modules = get_prev_modules(ep, types);

    for (auto module : prev_modules) {
      IntegerAllocatorRef int_allocator;

      switch (module->get_type()) {
      case ModuleType::Tofino_IntegerAllocatorAllocate:
      case ModuleType::Tofino_IntegerAllocatorQuery:
      case ModuleType::Tofino_IntegerAllocatorRejuvenate: {
        auto op = static_cast<IntegerAllocatorOperation *>(module.get());
        int_allocator = op->get_int_allocator();
      } break;
      default:
        assert(false && "Something went wrong...");
      }

      if (int_allocator->equals(target.get())) {
        return true;
      }
    }

    return false;
  }

public:
  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const IntegerAllocatorOperation *>(other);

    return int_allocator->equals(other_cast->get_int_allocator().get());
  }

  IntegerAllocatorRef get_int_allocator() const { return int_allocator; }
}; // namespace tofino

} // namespace tofino
} // namespace targets
} // namespace synapse
