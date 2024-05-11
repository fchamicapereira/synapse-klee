#include "symbex.h"

namespace bdd {

std::optional<addr_t> get_obj_from_call(const Call *node_call) {
  std::optional<addr_t> addr;

  assert(node_call);
  const call_t &call = node_call->get_call();

  klee::ref<klee::Expr> obj;

  if (call.function_name == "vector_borrow" ||
      call.function_name == "vector_return") {
    obj = call.args.at("vector").expr;
    assert(!obj.isNull());
  }

  else if (call.function_name == "map_get" || call.function_name == "map_put") {
    obj = call.args.at("map").expr;
    assert(!obj.isNull());
  }

  else if (call.function_name == "dchain_allocate_new_index" ||
           call.function_name == "dchain_is_index_allocated" ||
           call.function_name == "dchain_rejuvenate_index") {
    obj = call.args.at("chain").expr;
    assert(!obj.isNull());
  }

  if (!obj.isNull())
    addr = kutil::expr_addr_to_obj_addr(obj);

  return addr;
}

dchain_config_t get_dchain_config(const BDD &bdd, addr_t dchain_addr) {
  const std::vector<call_t> &init = bdd.get_init();

  for (const call_t &call : init) {
    if (call.function_name != "dchain_allocate")
      continue;

    klee::ref<klee::Expr> chain_out = call.args.at("chain_out").out;
    klee::ref<klee::Expr> index_range = call.args.at("index_range").expr;

    assert(!chain_out.isNull());
    assert(!index_range.isNull());

    addr_t chain_out_addr = kutil::expr_addr_to_obj_addr(chain_out);

    if (chain_out_addr != dchain_addr)
      continue;

    uint64_t index_range_value =
        kutil::solver_toolbox.value_from_expr(index_range);
    return dchain_config_t{index_range_value};
  }

  assert(false && "Should have found dchain configuration");
}

bits_t get_key_size(const BDD &bdd, addr_t addr) {
  const std::vector<call_t> &init = bdd.get_init();

  for (const call_t &call : init) {
    if (call.function_name == "map_allocate") {
      klee::ref<klee::Expr> _map = call.args.at("map_out").out;
      assert(!_map.isNull());

      addr_t _map_addr = kutil::expr_addr_to_obj_addr(_map);
      if (_map_addr != addr)
        continue;

      klee::ref<klee::Expr> key_size = call.args.at("key_size").expr;
      assert(!key_size.isNull());

      return kutil::solver_toolbox.value_from_expr(key_size);
    }

    if (call.function_name == "sketch_allocate") {
      klee::ref<klee::Expr> _sketch = call.args.at("sketch").expr;
      assert(!_sketch.isNull());

      addr_t _sketch_addr = kutil::expr_addr_to_obj_addr(_sketch);
      if (_sketch_addr != addr)
        continue;

      klee::ref<klee::Expr> key_size = call.args.at("key_size").expr;
      assert(!key_size.isNull());

      return kutil::solver_toolbox.value_from_expr(key_size);
    }
  }

  assert(false && "Should have found at least one node with a key");
}

map_config_t get_map_config(const BDD &bdd, addr_t map_addr) {
  const std::vector<call_t> &init = bdd.get_init();

  for (const call_t &call : init) {
    if (call.function_name != "map_allocate")
      continue;

    klee::ref<klee::Expr> capacity = call.args.at("capacity").expr;
    klee::ref<klee::Expr> key_size = call.args.at("key_size").expr;
    klee::ref<klee::Expr> map_out = call.args.at("map_out").out;

    assert(!capacity.isNull());
    assert(!key_size.isNull());
    assert(!map_out.isNull());

    addr_t map_out_addr = kutil::expr_addr_to_obj_addr(map_out);
    if (map_out_addr != map_addr)
      continue;

    uint64_t capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    bits_t key_size_value = kutil::solver_toolbox.value_from_expr(key_size) * 8;

    return map_config_t{capacity_value, static_cast<bits_t>(key_size_value)};
  }

  assert(false && "Should have found map configuration");
}

vector_config_t get_vector_config(const BDD &bdd, addr_t vector_addr) {
  const std::vector<call_t> &init = bdd.get_init();

  for (const call_t &call : init) {
    if (call.function_name != "vector_allocate")
      continue;

    klee::ref<klee::Expr> capacity = call.args.at("capacity").expr;
    klee::ref<klee::Expr> elem_size = call.args.at("elem_size").expr;
    klee::ref<klee::Expr> vector_out = call.args.at("vector_out").out;

    assert(!capacity.isNull());
    assert(!elem_size.isNull());
    assert(!vector_out.isNull());

    addr_t vector_out_addr = kutil::expr_addr_to_obj_addr(vector_out);
    if (vector_out_addr != vector_addr)
      continue;

    uint64_t capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    bits_t elem_size_value =
        kutil::solver_toolbox.value_from_expr(elem_size) * 8;

    return vector_config_t{capacity_value, elem_size_value};
  }

  assert(false && "Should have found vector configuration");
}

sketch_config_t get_sketch_config(const BDD &bdd, addr_t sketch_addr) {
  const std::vector<call_t> &init = bdd.get_init();

  for (const call_t &call : init) {
    if (call.function_name != "sketch_allocate")
      continue;

    klee::ref<klee::Expr> capacity = call.args.at("capacity").expr;
    klee::ref<klee::Expr> threshold = call.args.at("threshold").expr;
    klee::ref<klee::Expr> key_size = call.args.at("key_size").expr;
    klee::ref<klee::Expr> sketch_out = call.args.at("sketch_out").out;

    assert(!capacity.isNull());
    assert(!threshold.isNull());
    assert(!key_size.isNull());
    assert(!sketch_out.isNull());

    addr_t sketch_out_addr = kutil::expr_addr_to_obj_addr(sketch_out);
    if (sketch_out_addr != sketch_addr)
      continue;

    uint64_t capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    uint64_t threshold_value = kutil::solver_toolbox.value_from_expr(threshold);
    bits_t key_size_value = kutil::solver_toolbox.value_from_expr(key_size) * 8;

    return sketch_config_t{capacity_value, threshold_value, key_size_value};
  }

  assert(false && "Should have found sketch configuration");
}

cht_config_t get_cht_config(const BDD &bdd, addr_t cht_addr) {
  const std::vector<call_t> &init = bdd.get_init();

  for (const call_t &call : init) {
    if (call.function_name != "cht_fill_cht")
      continue;

    klee::ref<klee::Expr> capacity = call.args.at("backend_capacity").expr;
    klee::ref<klee::Expr> height = call.args.at("cht_height").expr;
    klee::ref<klee::Expr> cht = call.args.at("cht").expr;

    assert(!capacity.isNull());
    assert(!height.isNull());
    assert(!cht.isNull());

    addr_t _cht_addr = kutil::expr_addr_to_obj_addr(cht);
    if (_cht_addr != cht_addr)
      continue;

    uint64_t capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    uint64_t height_value = kutil::solver_toolbox.value_from_expr(height);

    return cht_config_t{capacity_value, height_value};
  }

  assert(false && "Should have found cht configuration");
}

} // namespace bdd