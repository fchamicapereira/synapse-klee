#pragma once

#include "mergeable_table_lookup.h"

namespace synapse {
namespace targets {
namespace tofino {

class TableLookup : public MergeableTableLookup {
public:
  TableLookup()
      : MergeableTableLookup(ModuleType::Tofino_TableLookup, "TableLookup") {}

  TableLookup(BDD::Node_ptr node, TableRef _table)
      : MergeableTableLookup(ModuleType::Tofino_TableLookup, "TableLookup",
                             node, _table) {}

private:
  TableRef build_non_mergeable_table(const BDD::Call *casted) const {
    auto data = get_extract_data(casted);

    if (!data.valid) {
      return TableRef();
    }

    auto node_id = casted->get_id();

    std::vector<BDD::symbol_t> hit;

    if (data.hit.first) {
      hit.push_back(data.hit.second);
    }

    auto table = TableNonMergeable::build(data.keys, data.values, hit,
                                          {data.obj}, {node_id});

    return table;
  }

  virtual bool lookup_already_done(const ExecutionPlan &ep,
                                   TableRef target) const override {
    auto prev_modules = get_prev_modules(ep, {ModuleType::Tofino_TableLookup});

    for (auto module : prev_modules) {
      auto lookup = static_cast<TableLookup *>(module.get());
      auto table = lookup->get_table();

      if (table->equals(target.get())) {
        return true;
      }
    }

    return false;
  }

  virtual bool check_compatible_placements_decisions(
      const ExecutionPlan &ep,
      const std::unordered_set<obj_addr_t> &objs) const override {
    auto mb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);

    for (auto obj : objs) {
      auto compatible = mb->check_implementation_compatibility(
          obj, {DataStructure::Type::TABLE_NON_MERGEABLE});

      if (!compatible) {
        return false;
      }
    }

    return true;
  }

  virtual bool process(const ExecutionPlan &ep, BDD::Node_ptr node,
                       TableRef table, processing_result_t &result) override {
    auto new_module = std::make_shared<TableLookup>(node, table);
    auto new_ep = ep.add_leaves(new_module, node->get_next());

    save_decision(new_ep, table);

    result.module = new_module;
    result.next_eps.push_back(new_ep);

    return true;
  }

  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto casted = BDD::cast_node<BDD::Call>(node);

    if (!casted) {
      return result;
    }

    auto table = build_non_mergeable_table(casted);

    if (!table) {
      return result;
    }

    if (!check_compatible_placements_decisions(ep, table->get_objs())) {
      return result;
    }

    process(ep, node, table, result);

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new TableLookup(node, table);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    return MergeableTableLookup::equals(other);
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
