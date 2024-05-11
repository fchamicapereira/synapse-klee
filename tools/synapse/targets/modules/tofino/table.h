#pragma once

#include "ignore.h"
#include "tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

class TableModule : public TofinoModule {
protected:
  TableRef table;

public:
  TableModule(Module::ModuleType table_module_type,
              const char *table_module_name)
      : TofinoModule(table_module_type, table_module_name) {}

  TableModule(Module::ModuleType table_module_type,
              const char *table_module_name, bdd::Node_ptr node,
              TableRef _table)
      : TofinoModule(table_module_type, table_module_name, node),
        table(_table) {}

protected:
  struct extracted_data_t {
    bool valid;
    addr_t obj;
    std::vector<Table::key_t> keys;
    std::vector<Table::param_t> values;
    std::vector<bdd::symbol_t> hit;
    std::unordered_set<bdd::node_id_t> nodes;

    extracted_data_t() : valid(false) {}
  };

  extracted_data_t extract_from_map_get(const bdd::Call *casted) const {
    auto call = casted->get_call();
    extracted_data_t data;

    if (call.function_name != "map_get") {
      return data;
    }

    assert(call.function_name == "map_get");
    assert(!call.args["map"].expr.isNull());
    assert(!call.args["key"].in.isNull());
    assert(!call.args["value_out"].out.isNull());

    auto _map = call.args["map"].expr;
    auto _key = call.args["key"].in;
    auto _key_meta = call.args["key"].meta;
    auto _value = call.args["value_out"].out;

    auto symbols = casted->get_locally_generated_symbols();
    assert(symbols.size() == 2);

    auto symbols_it = symbols.begin();
    assert(symbols_it->label_base == "map_has_this_key");
    auto _map_has_this_key = *symbols_it;

    auto _map_addr = kutil::expr_addr_to_obj_addr(_map);

    data.valid = true;
    data.obj = _map_addr;
    data.keys.emplace_back(_key, _key_meta);
    data.values.emplace_back(_map_addr, _value);
    data.hit.push_back(_map_has_this_key);
    data.nodes.insert(casted->get_id());

    return data;
  }

  extracted_data_t extract_from_vector_borrow(const bdd::Call *casted) const {
    auto call = casted->get_call();
    extracted_data_t data;

    if (call.function_name != "vector_borrow") {
      return data;
    }

    assert(call.function_name == "vector_borrow");
    assert(!call.args["vector"].expr.isNull());
    assert(!call.args["index"].expr.isNull());
    assert(!call.extra_vars["borrowed_cell"].second.isNull());

    auto _vector = call.args["vector"].expr;
    auto _index = call.args["index"].expr;
    auto _borrowed_cell = call.extra_vars["borrowed_cell"].second;

    auto _vector_addr = kutil::expr_addr_to_obj_addr(_vector);

    data.valid = true;
    data.obj = _vector_addr;
    data.keys.emplace_back(_index);
    data.values.emplace_back(_vector_addr, _borrowed_cell);
    data.nodes.insert(casted->get_id());

    return data;
  }

  extracted_data_t
  extract_from_dchain_rejuvenate(const bdd::Call *casted) const {
    auto call = casted->get_call();
    extracted_data_t data;

    if (call.function_name != "dchain_rejuvenate_index") {
      return data;
    }

    assert(!call.args["chain"].expr.isNull());
    assert(!call.args["index"].expr.isNull());
    assert(!call.args["time"].expr.isNull());

    auto _dchain = call.args["chain"].expr;
    auto _index = call.args["index"].expr;
    auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);

    data.valid = true;
    data.obj = _dchain_addr;
    data.keys.emplace_back(_index);
    data.nodes.insert(casted->get_id());

    return data;
  }

  extracted_data_t
  extract_from_dchain_is_index_allocated(const bdd::Call *casted) const {
    auto call = casted->get_call();
    extracted_data_t data;

    if (call.function_name != "dchain_is_index_allocated") {
      return data;
    }

    assert(!call.args["chain"].expr.isNull());
    assert(!call.args["index"].expr.isNull());

    auto _dchain = call.args["chain"].expr;
    auto _index = call.args["index"].expr;
    auto _generated_symbols = casted->get_locally_generated_symbols();

    auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);
    auto _is_allocated =
        get_symbol(_generated_symbols, "dchain_is_index_allocated");

    data.valid = true;
    data.obj = _dchain_addr;
    data.keys.emplace_back(_index);
    data.hit.push_back(_is_allocated);
    data.nodes.insert(casted->get_id());

    return data;
  }

  // We can only coalesce maps, vectors, and dchains if we always access vectors
  // with the index stored in the map.
  // The dchain indexes should be hidden inside a coalesced map. Checking if
  // the index itself is allocated breaks coalescing, as we should only access
  // the table through its keys.
  bool can_coalesce(const ExecutionPlan &ep,
                    const map_coalescing_data_t &data) const {
    const auto &bdd = ep.get_bdd();
    auto root = bdd.get_process();

    auto is_index_allocated_nodes =
        get_all_functions_after_node(root, {"dchain_is_index_allocated"});

    for (auto node : is_index_allocated_nodes) {
      auto call_node = bdd::cast_node<bdd::Call>(node);
      auto call = call_node->get_call();

      assert(!call.args["chain"].expr.isNull());

      auto _dchain = call.args["chain"].expr;
      auto _dchain_addr = kutil::expr_addr_to_obj_addr(_dchain);

      if (_dchain_addr == data.dchain) {
        return false;
      }
    }

    return true;
  }

  std::vector<extracted_data_t>
  get_incoming_vector_borrows(const bdd::Call *casted,
                              const map_coalescing_data_t &data) const {
    std::vector<extracted_data_t> vector_borrows;

    auto root = casted->get_next();
    auto incoming = get_all_functions_after_node(root, {"vector_borrow"});

    for (auto node : incoming) {
      auto call_node = bdd::cast_node<bdd::Call>(node);

      // Might as well use this
      auto extracted = extract_from_vector_borrow(call_node);
      assert(extracted.valid);

      if (data.vectors.find(extracted.obj) != data.vectors.end()) {
        vector_borrows.push_back(extracted);
      }
    }

    return vector_borrows;
  }

  void coalesce_with_incoming_vector_nodes(
      const bdd::Call *casted, const map_coalescing_data_t &coalescing_data,
      extracted_data_t &data) const {
    auto vector_borrows_data =
        get_incoming_vector_borrows(casted, coalescing_data);

    for (const auto &new_data : vector_borrows_data) {
      data.values.insert(data.values.end(), new_data.values.begin(),
                         new_data.values.end());
      data.nodes.insert(new_data.nodes.begin(), new_data.nodes.end());
    }
  }

  bool
  check_compatible_placements_decisions(const ExecutionPlan &ep,
                                        const std::unordered_set<addr_t> &objs,
                                        DataStructure::Type type) const {
    auto mb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);

    for (auto obj : objs) {
      auto compatible = mb->check_implementation_compatibility(obj, {type});

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

  bool already_coalesced(const ExecutionPlan &ep, DataStructure::Type type,
                         bdd::Node_ptr node) const {
    auto mb = ep.get_memory_bank<TofinoMemoryBank>(Tofino);
    auto impls = mb->get_implementations(type);

    for (auto impl : impls) {
      auto nodes = impl->get_nodes();

      if (nodes.find(node->get_id()) != nodes.end()) {
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

    auto other_cast = static_cast<const TableModule *>(other);

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
