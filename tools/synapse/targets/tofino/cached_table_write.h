#pragma once

#include "tofino_module.h"

#include "if.h"
#include "then.h"
#include "else.h"
#include "send_to_controller.h"

namespace synapse {
namespace tofino {

class CachedTableWrite : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> read_value;
  klee::ref<klee::Expr> write_value;
  symbol_t map_has_this_key;
  symbol_t cache_update_failed;

public:
  CachedTableWrite(const bdd::Node *node, DS_ID _cached_table_id,
                   const std::unordered_set<DS_ID> &_cached_table_byproducts,
                   addr_t _obj, klee::ref<klee::Expr> _key,
                   klee::ref<klee::Expr> _read_value,
                   klee::ref<klee::Expr> _write_value,
                   const symbol_t &_map_has_this_key,
                   const symbol_t &_cache_update_failed)
      : TofinoModule(ModuleType::Tofino_CachedTableWrite, "CachedTableWrite",
                     node),
        cached_table_id(_cached_table_id),
        cached_table_byproducts(_cached_table_byproducts), obj(_obj), key(_key),
        read_value(_read_value), write_value(_write_value),
        map_has_this_key(_map_has_this_key),
        cache_update_failed(_cache_update_failed) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new CachedTableWrite(
        node, cached_table_id, cached_table_byproducts, obj, key, read_value,
        write_value, map_has_this_key, cache_update_failed);
    return cloned;
  }

  DS_ID get_id() const { return cached_table_id; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_read_value() const { return read_value; }
  klee::ref<klee::Expr> get_write_value() const { return write_value; }
  const symbol_t &get_map_has_this_key() const { return map_has_this_key; }
  const symbol_t &get_cache_update_failed() const {
    return cache_update_failed;
  }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return cached_table_byproducts;
  }
};

class CachedTableWriteGenerator : public TofinoModuleGenerator {
public:
  CachedTableWriteGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_CachedTableWrite,
                              "CachedTableWrite") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_put") {
      return std::nullopt;
    }

    map_coalescing_data_t coalescing_data;
    if (!can_place_cached_table(ep, call_node, coalescing_data)) {
      return std::nullopt;
    }

    // TODO: Implement speculative processing
    return std::nullopt;

    return current_speculative_ctx;
  }

  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_put") {
      return new_eps;
    }

    map_coalescing_data_t coalescing_data;
    if (!can_place_cached_table(ep, call_node, coalescing_data)) {
      return new_eps;
    }

    assert(false && "TODO");

    return new_eps;
  }
};

} // namespace tofino
} // namespace synapse
