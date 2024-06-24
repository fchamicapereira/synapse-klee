#pragma once

#include "tofino_module.h"

#include "if.h"
#include "then.h"
#include "else.h"
#include "send_to_controller.h"

namespace synapse {
namespace tofino {

class CachedTableConditionalWrite : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> read_value;
  klee::ref<klee::Expr> write_value;
  symbol_t map_has_this_key;
  symbol_t cache_write_failed;

public:
  CachedTableConditionalWrite(
      const bdd::Node *node, DS_ID _cached_table_id,
      const std::unordered_set<DS_ID> &_cached_table_byproducts, addr_t _obj,
      klee::ref<klee::Expr> _key, klee::ref<klee::Expr> _read_value,
      klee::ref<klee::Expr> _write_value, const symbol_t &_map_has_this_key,
      const symbol_t &_cache_write_failed)
      : TofinoModule(ModuleType::Tofino_CachedTableConditionalWrite,
                     "CachedTableConditionalWrite", node),
        cached_table_id(_cached_table_id),
        cached_table_byproducts(_cached_table_byproducts), obj(_obj), key(_key),
        read_value(_read_value), write_value(_write_value),
        map_has_this_key(_map_has_this_key),
        cache_write_failed(_cache_write_failed) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new CachedTableConditionalWrite(
        node, cached_table_id, cached_table_byproducts, obj, key, read_value,
        write_value, map_has_this_key, cache_write_failed);
    return cloned;
  }

  DS_ID get_id() const { return cached_table_id; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_read_value() const { return read_value; }
  klee::ref<klee::Expr> get_write_value() const { return write_value; }
  const symbol_t &get_map_has_this_key() const { return map_has_this_key; }
  const symbol_t &get_cache_write_failed() const { return cache_write_failed; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return cached_table_byproducts;
  }
};

class CachedTableConditionalWriteGenerator : public TofinoModuleGenerator {
public:
  CachedTableConditionalWriteGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_CachedTableConditionalWrite,
                              "CachedTableConditionalWrite") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_get") {
      return new_eps;
    }

    map_coalescing_data_t coalescing_data;
    if (!can_place_cached_table(ep, call_node, coalescing_data)) {
      return new_eps;
    }

    std::vector<const bdd::Call *> future_map_puts;
    if (!is_map_get_followed_by_map_puts_on_miss(ep->get_bdd(), call_node,
                                                 future_map_puts)) {
      // The cached table read should deal with these cases.
      return new_eps;
    }

    cached_table_data_t data =
        get_cached_table_data(ep, call_node, future_map_puts);

    std::unordered_set<DS_ID> deps;
    int cache_capacity = 1024;
    bool already_exists;
    CachedTable *cached_table = get_or_build_cached_table(
        ep, node, data, cache_capacity, already_exists, deps);

    if (!cached_table) {
      return new_eps;
    }

    symbol_t cache_write_failed = create_cache_write_failed_symbol();

    std::unordered_set<DS_ID> byproducts =
        get_cached_table_byproducts(cached_table);

    klee::ref<klee::Expr> cache_write_success_condition =
        build_cache_write_success_condition(cache_write_failed);

    Module *module = new CachedTableConditionalWrite(
        node, cached_table->id, byproducts, data.obj, data.key, data.read_value,
        data.write_value, data.map_has_this_key, cache_write_failed);
    EPNode *cached_table_cond_write_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    bdd::Node *on_cache_write_success;
    bdd::Node *on_cache_write_failed;
    std::optional<constraints_t> deleted_branch_constraints;

    bdd::BDD *new_bdd = branch_bdd_on_cache_write_success(
        new_ep, node, data, cache_write_success_condition, coalescing_data,
        on_cache_write_success, on_cache_write_failed,
        deleted_branch_constraints);

    symbols_t symbols = get_dataplane_state(ep, node);

    Module *if_module = new If(node, cache_write_success_condition,
                               {cache_write_success_condition});
    Module *then_module = new Then(node);
    Module *else_module = new Else(node);
    Module *send_to_controller_module =
        new SendToController(on_cache_write_failed, symbols);

    EPNode *if_node = new EPNode(if_module);
    EPNode *then_node = new EPNode(then_module);
    EPNode *else_node = new EPNode(else_module);
    EPNode *send_to_controller_node = new EPNode(send_to_controller_module);

    cached_table_cond_write_node->set_children({if_node});
    if_node->set_prev(cached_table_cond_write_node);

    if_node->set_children({then_node, else_node});
    then_node->set_prev(if_node);
    else_node->set_prev(if_node);

    else_node->set_children({send_to_controller_node});
    send_to_controller_node->set_prev(else_node);

    EPLeaf on_cache_write_success_leaf(then_node, on_cache_write_success);
    EPLeaf on_cache_write_failed_leaf(send_to_controller_node,
                                      on_cache_write_failed);

    float cache_write_success_estimation_rel =
        get_cache_write_success_estimation_rel(data, cache_capacity);

    new_ep->update_node_constraints(then_node, else_node,
                                    cache_write_success_condition);

    update_profiler(new_ep, cache_write_success_condition,
                    cache_write_success_estimation_rel,
                    deleted_branch_constraints);

    new_ep->process_leaf(
        cached_table_cond_write_node,
        {on_cache_write_success_leaf, on_cache_write_failed_leaf});
    new_ep->replace_bdd(new_bdd);

    new_ep->inspect();

    if (!already_exists) {
      place_cached_table(new_ep, coalescing_data, cached_table, deps);
    }

    return new_eps;
  }

private:
  void update_profiler(
      EP *ep, klee::ref<klee::Expr> cache_write_success_condition,
      float cache_write_success_estimation_rel,
      const std::optional<constraints_t> &deleted_branch_constraints) const {
    ep->add_hit_rate_estimation(cache_write_success_condition,
                                cache_write_success_estimation_rel);

    if (deleted_branch_constraints.has_value()) {
      ep->remove_hit_rate_node(deleted_branch_constraints.value());
    }
  }

  float get_cache_write_success_estimation_rel(
      const cached_table_data_t &cached_table_data, int cache_capacity) const {
    return static_cast<float>(cache_capacity) / cached_table_data.num_entries;
  }

  std::unordered_set<DS_ID>
  get_cached_table_byproducts(CachedTable *cached_table) const {
    std::unordered_set<DS_ID> byproducts;

    std::vector<std::unordered_set<const DS *>> internal_ds =
        cached_table->get_internal_ds();
    for (const std::unordered_set<const DS *> &ds_set : internal_ds) {
      for (const DS *ds : ds_set) {
        byproducts.insert(ds->id);
      }
    }

    return byproducts;
  }

  cached_table_data_t
  get_cached_table_data(const EP *ep, const bdd::Call *map_get,
                        std::vector<const bdd::Call *> future_map_puts) const {
    cached_table_data_t data;

    assert(!future_map_puts.empty());
    const bdd::Call *map_put = future_map_puts.front();

    const call_t &get_call = map_get->get_call();
    const call_t &put_call = map_put->get_call();

    symbols_t symbols = map_get->get_locally_generated_symbols();
    klee::ref<klee::Expr> obj_expr = get_call.args.at("map").expr;

    data.obj = kutil::expr_addr_to_obj_addr(obj_expr);
    data.key = get_call.args.at("key").in;
    data.read_value = get_call.args.at("value_out").out;
    data.write_value = put_call.args.at("value").expr;

    bool found = get_symbol(symbols, "map_has_this_key", data.map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(data.obj);

    data.num_entries = cfg.capacity;
    return data;
  }

  klee::ref<klee::Expr> build_cache_write_success_condition(
      const symbol_t &cache_write_failed) const {
    klee::ref<klee::Expr> zero = kutil::solver_toolbox.exprBuilder->Constant(
        0, cache_write_failed.expr->getWidth());
    return kutil::solver_toolbox.exprBuilder->Eq(cache_write_failed.expr, zero);
  }

  symbol_t create_cache_write_failed_symbol() const {
    const klee::Array *array;
    std::string label = "cache_write_failed";
    bits_t size = 32;

    klee::ref<klee::Expr> expr =
        kutil::solver_toolbox.create_new_symbol(label, size, array);

    symbol_t cache_write_failed = {
        .base = label,
        .array = array,
        .expr = expr,
    };

    return cache_write_failed;
  }

  void add_map_get_clone_on_cache_write_failed(
      const EP *ep, bdd::BDD *bdd, const bdd::Node *map_get,
      const bdd::Branch *cache_write_branch,
      bdd::Node *&new_on_cache_write_failed) const {
    bdd::node_id_t &id = bdd->get_mutable_id();
    bdd::NodeManager &manager = bdd->get_mutable_manager();
    bdd::Node *map_get_on_cache_write_failed = map_get->clone(manager, false);
    map_get_on_cache_write_failed->recursive_update_ids(id);

    add_non_branch_nodes_to_bdd(ep, bdd, cache_write_branch->get_on_false(),
                                {map_get_on_cache_write_failed},
                                new_on_cache_write_failed);
  }

  void replicate_hdr_parsing_ops_on_cache_write_failed(
      const EP *ep, bdd::BDD *bdd, const bdd::Branch *cache_write_branch,
      bdd::Node *&new_on_cache_write_failed) const {
    const bdd::Node *on_cache_write_failed = cache_write_branch->get_on_false();

    std::vector<const bdd::Node *> prev_borrows = get_prev_functions(
        ep, on_cache_write_failed, {"packet_borrow_next_chunk"});

    if (prev_borrows.empty()) {
      return;
    }

    add_non_branch_nodes_to_bdd(ep, bdd, on_cache_write_failed, prev_borrows,
                                new_on_cache_write_failed);
  }

  void delete_coalescing_nodes_on_success(
      const EP *ep, bdd::BDD *bdd, bdd::Node *on_success,
      const map_coalescing_data_t &coalescing_data, klee::ref<klee::Expr> key,
      std::optional<constraints_t> &deleted_branch_constraints) const {
    const std::vector<const bdd::Node *> targets =
        get_coalescing_nodes_from_key(bdd, on_success, key, coalescing_data);

    for (const bdd::Node *target : targets) {
      const bdd::Call *call_target = static_cast<const bdd::Call *>(target);
      const call_t &call = call_target->get_call();

      if (call.function_name == "dchain_allocate_new_index") {
        symbol_t out_of_space;
        bool found = get_symbol(call_target->get_locally_generated_symbols(),
                                "out_of_space", out_of_space);
        assert(found && "Symbol out_of_space not found");

        const bdd::Branch *branch =
            find_branch_checking_index_alloc(ep, on_success, out_of_space);

        if (branch) {
          assert(!deleted_branch_constraints.has_value() &&
                 "Multiple branch checking index allocation detected");
          deleted_branch_constraints = branch->get_ordered_branch_constraints();
          bool direction_to_keep = true;

          klee::ref<klee::Expr> extra_constraint = branch->get_condition();

          // If we want to keep the direction on true, we must remove the on
          // false.
          if (direction_to_keep) {
            extra_constraint =
                kutil::solver_toolbox.exprBuilder->Not(extra_constraint);
          }
          deleted_branch_constraints->push_back(extra_constraint);

          bdd::Node *trash;
          delete_branch_node_from_bdd(ep, bdd, branch, direction_to_keep,
                                      trash);
        }
      }

      bdd::Node *trash;
      delete_non_branch_node_from_bdd(ep, bdd, target, trash);
    }
  }
  bdd::BDD *branch_bdd_on_cache_write_success(
      const EP *ep, const bdd::Node *map_get, const cached_table_data_t &data,
      klee::ref<klee::Expr> cache_write_success_condition,
      const map_coalescing_data_t &coalescing_data,
      bdd::Node *&on_cache_write_success, bdd::Node *&on_cache_write_failed,
      std::optional<constraints_t> &deleted_branch_constraints) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = map_get->get_next();
    assert(next);

    bdd::Branch *cache_write_branch;
    add_branch_to_bdd(ep, new_bdd, next, cache_write_success_condition,
                      cache_write_branch);
    on_cache_write_success = cache_write_branch->get_mutable_on_true();

    add_map_get_clone_on_cache_write_failed(
        ep, new_bdd, map_get, cache_write_branch, on_cache_write_failed);
    replicate_hdr_parsing_ops_on_cache_write_failed(
        ep, new_bdd, cache_write_branch, on_cache_write_failed);

    delete_coalescing_nodes_on_success(ep, new_bdd, on_cache_write_success,
                                       coalescing_data, data.key,
                                       deleted_branch_constraints);

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
