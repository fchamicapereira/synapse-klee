#pragma once

#include "table.h"

namespace synapse {
namespace targets {
namespace tofino {

class TableRejuvenation : public TableModule {
public:
  TableRejuvenation()
      : TableModule(ModuleType::Tofino_TableRejuvenation, "TableRejuvenation") {
  }

  TableRejuvenation(BDD::Node_ptr node, TableRef _table)
      : TableModule(ModuleType::Tofino_TableRejuvenation, "TableRejuvenation",
                    node, _table) {}

protected:
  extracted_data_t extract_data(const ExecutionPlan &ep,
                                BDD::Node_ptr node) const {
    auto extractors = {
        &TableRejuvenation::extract_from_dchain_rejuvenate,
    };

    extracted_data_t data;

    auto casted = BDD::cast_node<BDD::Call>(node);

    if (!casted) {
      return data;
    }

    for (auto &extractor : extractors) {
      data = (this->*extractor)(casted);

      if (data.valid) {
        break;
      }
    }

    return data;
  }

  TableRef get_or_build_table(const ExecutionPlan &ep,
                              const extracted_data_t &data) const {
    auto tmb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);

    const auto &impls = tmb->get_implementations(data.obj);

    // TableIsAllocated module can also implement dchains, but only
    // is_index_allocated operations. This means we are expected to find the
    // same dchain associated with multiple implementations.
    for (auto impl : impls) {
      assert(impl->get_type() == DataStructure::Type::TABLE);

      auto table = std::dynamic_pointer_cast<Table>(impls[0]);

      if (!table->is_managing_expirations()) {
        continue;
      }

      table->add_nodes(data.nodes);

      auto clone = table->clone();
      clone->change_keys(data.keys);

      return clone;
    }

    auto mb = ep.get_memory_bank();
    auto expiration_data = mb->get_expiration_data();
    assert(expiration_data.valid);

    auto table = Table::build_for_obj("rejuvenator", data.keys, data.values,
                                      data.hit, {data.obj}, data.nodes);
    table->should_manage_expirations(expiration_data.expiration_time);

    return table;
  }

  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto data = extract_data(ep, node);

    if (!data.valid) {
      return result;
    }

    if (!check_compatible_placements_decisions(ep, {data.obj},
                                               DataStructure::Type::TABLE)) {
      return result;
    }

    auto table = get_or_build_table(ep, data);

    if (!table) {
      return result;
    }

    auto new_module = std::make_shared<TableRejuvenation>(node, table);

    result = postpone(ep, node, new_module);
    assert(result.next_eps.size() == 1);

    save_decision(*result.next_eps.begin(), table);

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new TableRejuvenation(node, table);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const TableRejuvenation *>(other);

    if (!table->equals(other_cast->get_table().get())) {
      return false;
    }

    return true;
  }

  TableRef get_table() const { return table; }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
