#include "memory_bank.h"
#include "../module.h"

namespace synapse {
namespace targets {
namespace tofino {

void TofinoMemoryBank::save_implementation(const DataStructureRef &ds) {
  implementations.insert(ds);
}

const std::vector<DataStructureRef> &
TofinoMemoryBank::get_implementations() const {
  return implementations.get();
}

std::vector<DataStructureRef>
TofinoMemoryBank::get_implementations(DataStructure::Type ds_type) const {
  return implementations.get(ds_type);
}

std::vector<DataStructureRef>
TofinoMemoryBank::get_implementations(addr_t obj) const {
  return implementations.get(obj);
}

bool TofinoMemoryBank::check_implementation_compatibility(
    addr_t obj,
    const std::vector<DataStructure::Type> &compatible_types) const {
  DataStructureRef implementation;

  auto this_impls = get_implementations(obj);

  if (this_impls.size() == 0) {
    return true;
  }

  for (auto impl : this_impls) {
    auto found_it = std::find(compatible_types.begin(), compatible_types.end(),
                              impl->get_type());

    if (found_it == compatible_types.end()) {
      return false;
    }
  }

  return true;
}

void TofinoMemoryBank::add_dataplane_state(const BDD::symbols_t &symbols) {
  for (const auto &symbol : symbols) {
    dp_state.insert(symbol);
  }
}

const BDD::symbols_t &TofinoMemoryBank::get_dataplane_state() const {
  return dp_state;
}

void TofinoMemoryBank::postpone(BDD::node_id_t node_id, Module_ptr module) {
  postponed.emplace_back(node_id, module);
}

const std::vector<postponed_t> &TofinoMemoryBank::get_postponed() const {
  return postponed;
}

} // namespace tofino
} // namespace targets
} // namespace synapse