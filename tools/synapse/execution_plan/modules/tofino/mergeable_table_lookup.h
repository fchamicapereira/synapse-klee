#pragma once

#include "ignore.h"
#include "tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

class MergeableTableLookup : public TofinoModule {
protected:
  TableRef table;

public:
  MergeableTableLookup()
      : TofinoModule(ModuleType::Tofino_MergeableTableLookup,
                     "MergeableTableLookup") {}

  MergeableTableLookup(BDD::Node_ptr node, TableRef _table)
      : TofinoModule(ModuleType::Tofino_MergeableTableLookup,
                     "MergeableTableLookup", node),
        table(_table) {
    assert(table);
  }

  /*
    These additional constructors are for other modules who derive from
    MergeableTableLookup, like MergeableTableLookupSimple (which does not try to
    perform table merging and coalescing).
  */
  MergeableTableLookup(Module::ModuleType table_module_type,
                       const char *table_module_name)
      : TofinoModule(table_module_type, table_module_name) {}

  MergeableTableLookup(Module::ModuleType table_module_type,
                       const char *table_module_name, BDD::Node_ptr node,
                       TableRef _table)
      : TofinoModule(table_module_type, table_module_name, node),
        table(_table) {}

protected:
  struct extracted_data_t {
    bool valid;
    std::string fname;
    obj_addr_t obj;
    std::vector<Table::key_t> keys;
    std::vector<Table::param_t> values;
    std::pair<bool, BDD::symbol_t> hit;

    extracted_data_t() : valid(false) {}
    extracted_data_t(const std::string &_fname) : valid(false), fname(_fname) {}
  };

  extracted_data_t extract_from_map(const BDD::Call *casted) const {
    auto call = casted->get_call();
    extracted_data_t data(call.function_name);

    if (call.function_name != symbex::FN_MAP_GET) {
      return data;
    }

    assert(call.function_name == symbex::FN_MAP_GET);
    assert(!call.args[symbex::FN_MAP_ARG_MAP].expr.isNull());
    assert(!call.args[symbex::FN_MAP_ARG_KEY].in.isNull());
    assert(!call.args[symbex::FN_MAP_ARG_OUT].out.isNull());

    auto _map = call.args[symbex::FN_MAP_ARG_MAP].expr;
    auto _key = call.args[symbex::FN_MAP_ARG_KEY].in;
    auto _key_meta = call.args[symbex::FN_MAP_ARG_KEY].meta;
    auto _value = call.args[symbex::FN_MAP_ARG_OUT].out;

    auto symbols = casted->get_local_generated_symbols();
    assert(symbols.size() == 2);

    auto symbols_it = symbols.begin();
    assert(symbols_it->label_base == symbex::MAP_HAS_THIS_KEY);
    auto _map_has_this_key = *symbols_it;

    auto _map_addr = kutil::expr_addr_to_obj_addr(_map);

    data.valid = true;
    data.obj = _map_addr;
    data.keys.emplace_back(_key, _key_meta);
    data.values.emplace_back(_map_addr, _value);
    data.hit = std::pair<bool, BDD::symbol_t>{true, _map_has_this_key};

    return data;
  }

  extracted_data_t extract_from_vector(const BDD::Call *casted) const {
    auto call = casted->get_call();
    extracted_data_t data(call.function_name);

    if (call.function_name != symbex::FN_VECTOR_BORROW) {
      return data;
    }

    assert(call.function_name == symbex::FN_VECTOR_BORROW);
    assert(!call.args[symbex::FN_VECTOR_ARG_VECTOR].expr.isNull());
    assert(!call.args[symbex::FN_VECTOR_ARG_INDEX].expr.isNull());
    assert(!call.extra_vars[symbex::FN_VECTOR_EXTRA].second.isNull());

    auto _vector = call.args[symbex::FN_VECTOR_ARG_VECTOR].expr;
    auto _index = call.args[symbex::FN_VECTOR_ARG_INDEX].expr;
    auto _borrowed_cell = call.extra_vars[symbex::FN_VECTOR_EXTRA].second;

    auto _vector_addr = kutil::expr_addr_to_obj_addr(_vector);

    data.valid = true;
    data.obj = _vector_addr;
    data.keys.emplace_back(_index);
    data.values.emplace_back(_vector_addr, _borrowed_cell);

    return data;
  }

  virtual bool check_compatible_placements_decisions(
      const ExecutionPlan &ep,
      const std::unordered_set<obj_addr_t> &objs) const {
    auto mb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);

    for (auto obj : objs) {
      auto compatible = mb->check_implementation_compatibility(
          obj, {DataStructure::Type::TABLE});

      if (!compatible) {
        return false;
      }
    }

    return true;
  }

  void save_decision(const ExecutionPlan &ep, DataStructureRef table) const {
    auto mb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);
    mb->save_implementation(table);
  }

  extracted_data_t get_extract_data(const BDD::Call *casted) const {
    auto extractors = {
        &MergeableTableLookup::extract_from_map,
        &MergeableTableLookup::extract_from_vector,
    };

    extracted_data_t data;

    for (auto &extractor : extractors) {
      data = (this->*extractor)(casted);

      if (data.valid) {
        break;
      }
    }

    return data;
  }

  TableRef build_table(const BDD::Call *casted) const {
    auto data = get_extract_data(casted);

    if (!data.valid) {
      return TableRef();
    }

    auto node_id = casted->get_id();

    std::vector<BDD::symbol_t> hit;

    if (data.hit.first) {
      hit.push_back(data.hit.second);
    }

    auto table =
        Table::build(data.keys, data.values, hit, {data.obj}, {node_id});

    return table;
  }

  void
  remember_to_ignore_coalesced(const ExecutionPlan &ep,
                               const coalesced_data_t &coalesced_data) const {
    auto mb = ep.get_memory_bank();

    for (auto incoming : coalesced_data.vector_borrows) {
      mb->can_be_ignored(incoming);
    }
  }

  bool was_coalesced(const ExecutionPlan &ep, BDD::Node_ptr node) const {
    auto mb = ep.get_memory_bank();
    return mb->check_if_can_be_ignored(node);
  }

  void coalesce(TableRef &table, const coalesced_data_t &coalesced_data) const {
    for (auto vector : coalesced_data.vector_borrows) {
      assert(vector->get_type() == BDD::Node::CALL);
      auto call_node = static_cast<const BDD::Call *>(vector.get());
      auto vector_table = build_table(call_node);
      table->merge(vector_table.get());
    }
  }

  virtual bool lookup_already_done(const ExecutionPlan &ep,
                                   TableRef target) const {
    auto prev_modules =
        get_prev_modules(ep, {ModuleType::Tofino_MergeableTableLookup});

    for (auto module : prev_modules) {
      auto lookup = static_cast<MergeableTableLookup *>(module.get());
      auto table = lookup->get_table();

      if (table->equals(target.get())) {
        return true;
      }
    }

    return false;
  }

  // Returns a bool indicating if this lookup should be ignored, because
  // the table was merged with an already created table and a lookup was
  // already performed in this execution path.
  bool try_merge(const ExecutionPlan &ep, TableRef &table) {
    auto tmb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);

    auto objs = table->get_objs();
    assert(objs.size() > 0);
    auto obj = *objs.begin();

    auto implementations = tmb->get_implementations(obj);

    // Find the right table to reuse
    for (auto implementation : implementations) {
      if (implementation->get_type() != DataStructure::Type::TABLE) {
        continue;
      }

      if (!implementation->implements(objs)) {
        continue;
      }

      auto other_table = static_cast<Table *>(implementation.get());

      if (!other_table->can_merge(table.get())) {
        continue;
      }

      other_table->merge(table.get());

      // Use the other table instead, since we have merged them.
      table = std::dynamic_pointer_cast<Table>(implementation);

      return lookup_already_done(ep, table);
    }

    return false;
  }

  // We should ignore the rejuvenate operation if there is no other
  // implementation responsible for this object AND its expirations.
  bool should_ignore_rejuvenate(const ExecutionPlan &ep,
                                const BDD::Call *casted) const {
    auto call = casted->get_call();

    if (call.function_name != symbex::FN_DCHAIN_REJUVENATE) {
      return false;
    }

    assert(!call.args[symbex::FN_DCHAIN_ARG_CHAIN].expr.isNull());
    auto dchain = call.args[symbex::FN_DCHAIN_ARG_CHAIN].expr;
    auto dchain_addr = kutil::expr_addr_to_obj_addr(dchain);

    auto tmb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);

    // We are only interested in ignoring the rejuvenate operation
    // if the only implementation found is of a Table.
    auto compatible = tmb->check_implementation_compatibility(
        dchain_addr, {DataStructure::Type::TABLE});

    if (!compatible) {
      return false;
    }

    // By this point we have decided to manage expirations for this table.
    auto impls = tmb->get_implementations(dchain_addr);

    // If there are no implementations yet, we are not ready to ignore this
    // rejuvenate. We should only ignore if there are tables which implement it.
    if (impls.size() == 0) {
      return false;
    }

    assert(impls.size() <= 1);
    return false; // FIXME: remove this

    for (auto impl : impls) {
      // These should all be tables, by the previously performed compatibility
      // check.
      // FIXME: Should ALL of them manage expirations?
      assert(impl->get_type() == DataStructure::Type::TABLE);
      auto table = static_cast<Table *>(impl.get());
      table->should_manage_expirations();
      break;
    }

    return true;
  }

  virtual bool process(const ExecutionPlan &ep, BDD::Node_ptr node,
                       TableRef table, processing_result_t &result) {
    auto new_module = std::make_shared<MergeableTableLookup>(node, table);
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

    // No point in generating modules for already coalesced tables.
    if (was_coalesced(ep, node)) {
      return result;
    }

    if (should_ignore_rejuvenate(ep, casted)) {
      // If we are not using an IntegerAllocator to control expirations,
      // and if this table is coalesced with a dchain, then a simple
      // lookup makes a rejuvenation. Therefore there's no need for
      // another module for rejuvenation, and we should just ignore this BDD
      // node.

      auto new_module = std::make_shared<Ignore>(node);
      auto new_ep = ep.ignore_leaf(node->get_next(), TargetType::Tofino);

      result.module = new_module;
      result.next_eps.push_back(new_ep);

      return result;
    }

    auto table = build_table(casted);

    if (!table) {
      return result;
    }

    if (!check_compatible_placements_decisions(ep, table->get_objs())) {
      return result;
    }

    auto coalesced_data = get_coalescing_data(ep, node);

    auto can_coalesce =
        coalesced_data.can_coalesce &&
        check_compatible_placements_decisions(ep, coalesced_data.get_objs());

    if (can_coalesce) {
      coalesce(table, coalesced_data);
    }

    auto merged_and_skip_lookup = try_merge(ep, table);

    if (merged_and_skip_lookup) {
      // If we merged, then we should ignore this node and keep going,
      // as its information was added to a previous MergeableTableLookup
      // instance.

      auto new_module = std::make_shared<Ignore>(node);
      auto new_ep = ep.ignore_leaf(node->get_next(), TargetType::Tofino);

      result.module = new_module;
      result.next_eps.push_back(new_ep);

      return result;
    }

    process(ep, node, table, result);

    if (can_coalesce && result.module) {
      assert(result.next_eps.size());
      auto &new_ep = result.next_eps.back();
      remember_to_ignore_coalesced(new_ep, coalesced_data);
    }

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new MergeableTableLookup(node, table);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const MergeableTableLookup *>(other);

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
