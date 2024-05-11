#pragma once

#include <klee/Expr.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../../memory_bank.h"
#include "data_structures/data_structures.h"

namespace synapse {

class Module;
typedef std::shared_ptr<Module> Module_ptr;

namespace targets {
namespace tofino {

typedef std::pair<bdd::node_id_t, Module_ptr> postponed_t;

class TofinoMemoryBank : public TargetMemoryBank {
private:
  DataStructuresSet implementations;
  std::vector<postponed_t> postponed;
  bdd::symbols_t dp_state;

public:
  TofinoMemoryBank() {}

  TofinoMemoryBank(const TofinoMemoryBank &mb)
      : implementations(mb.implementations), postponed(mb.postponed),
        dp_state(mb.dp_state) {}

  void save_implementation(const DataStructureRef &ds);
  const std::vector<DataStructureRef> &get_implementations() const;

  std::vector<DataStructureRef>
  get_implementations(DataStructure::Type ds_type) const;

  std::vector<DataStructureRef> get_implementations(addr_t obj) const;

  bool check_implementation_compatibility(
      addr_t obj,
      const std::vector<DataStructure::Type> &compatible_types) const;

  void postpone(bdd::node_id_t node_id, Module_ptr module);
  const std::vector<postponed_t> &get_postponed() const;

  void add_dataplane_state(const bdd::symbols_t &symbols);
  const bdd::symbols_t &get_dataplane_state() const;

  virtual TargetMemoryBank_ptr clone() const override {
    auto clone = new TofinoMemoryBank(*this);
    return TargetMemoryBank_ptr(clone);
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse