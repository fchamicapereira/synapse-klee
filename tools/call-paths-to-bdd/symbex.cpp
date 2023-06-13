#include "symbex.h"

namespace BDD {
namespace symbex {

std::pair<bool, addr_t> get_obj_from_call(const Call *node_call) {
  std::pair<bool, addr_t> result;

  result.first = false;

  assert(node_call);
  auto call = node_call->get_call();

  klee::ref<klee::Expr> obj;

  if (call.function_name == FN_VECTOR_BORROW ||
      call.function_name == FN_VECTOR_RETURN) {
    assert(!call.args[FN_VECTOR_ARG_VECTOR].expr.isNull());
    obj = call.args[FN_VECTOR_ARG_VECTOR].expr;
  }

  else if (call.function_name == FN_MAP_GET ||
           call.function_name == FN_MAP_PUT) {
    assert(!call.args[FN_MAP_ARG_MAP].expr.isNull());
    obj = call.args[FN_MAP_ARG_MAP].expr;
  }

  else if (call.function_name == FN_DCHAIN_ALLOCATE_NEW_INDEX ||
           call.function_name == FN_DCHAIN_IS_ALLOCATED ||
           call.function_name == FN_DCHAIN_REJUVENATE) {
    assert(!call.args[FN_DCHAIN_ARG_CHAIN].expr.isNull());
    obj = call.args[FN_DCHAIN_ARG_CHAIN].expr;
  }

  if (!obj.isNull()) {
    auto addr = kutil::expr_addr_to_obj_addr(obj);
    result.first = true;
    result.second = addr;
  }

  return result;
}

dchain_config_t get_dchain_config(const BDD &bdd, addr_t dchain_addr) {
  auto init_nodes = get_call_nodes(bdd.get_init(), {FN_DCHAIN_ALLOCATE});

  assert(init_nodes.size() > 0);

  for (auto init_node : init_nodes) {
    auto call_node = cast_node<Call>(init_node);
    auto call = call_node->get_call();

    assert(!call.args[FN_DCHAIN_ALLOCATE_ARG_CHAIN_OUT].out.isNull());
    assert(!call.args[FN_DCHAIN_ALLOCATE_ARG_INDEX_RANGE].expr.isNull());

    auto chain_out = call.args[FN_DCHAIN_ALLOCATE_ARG_CHAIN_OUT].out;
    auto index_range = call.args[FN_DCHAIN_ALLOCATE_ARG_INDEX_RANGE].expr;

    auto chain_out_addr = kutil::expr_addr_to_obj_addr(chain_out);

    if (chain_out_addr != dchain_addr) {
      continue;
    }

    auto index_range_value = kutil::solver_toolbox.value_from_expr(index_range);
    return dchain_config_t{index_range_value};
  }

  assert(false && "Should have found dchain configuration");

  std::cerr << "Dchain configuration not found.\n";
  exit(1);
}

bits_t get_key_size(const BDD &bdd, addr_t addr) {
  auto nodes = get_call_nodes(
      bdd.get_process(), {FN_MAP_GET, FN_MAP_PUT, FN_SKETCH_COMPUTE_HASHES});

  assert(nodes.size() > 0);

  for (auto node : nodes) {
    auto call_node = cast_node<Call>(node);
    auto call = call_node->get_call();

    if (call.function_name == FN_MAP_GET || call.function_name == FN_MAP_PUT) {
      assert(!call.args[FN_MAP_ARG_MAP].expr.isNull());
      auto _map = call.args[FN_MAP_ARG_MAP].expr;
      auto _map_addr = kutil::expr_addr_to_obj_addr(_map);

      if (_map_addr != addr) {
        continue;
      }

      assert(!call.args[FN_MAP_ARG_KEY].in.isNull());
      auto _key = call.args[FN_MAP_ARG_KEY].in;

      return _key->getWidth();
    }

    else if (call.function_name == FN_SKETCH_COMPUTE_HASHES) {
      assert(!call.args[FN_SKETCH_ARG_SKETCH].expr.isNull());
      auto _sketch = call.args[FN_SKETCH_ARG_SKETCH].expr;
      auto _sketch_addr = kutil::expr_addr_to_obj_addr(_sketch);

      if (_sketch_addr != addr) {
        continue;
      }

      assert(!call.args[FN_SKETCH_ARG_KEY].in.isNull());
      auto _key = call.args[FN_SKETCH_ARG_KEY].in;

      return _key->getWidth();
    }
  }

  assert(false && "Should have found at least one node with a key");

  std::cerr << "Node not found but required to find key size..\n";
  exit(1);
}

map_config_t get_map_config(const BDD &bdd, addr_t map_addr) {
  auto init_nodes = get_call_nodes(bdd.get_init(), {FN_MAP_ALLOCATE});

  assert(init_nodes.size() > 0);

  for (auto init_node : init_nodes) {
    auto call_node = cast_node<Call>(init_node);
    auto call = call_node->get_call();

    assert(!call.args[FN_MAP_ARG_CAPACITY].expr.isNull());
    assert(call.args[FN_MAP_ARG_KEY_EQUAL].fn_ptr_name.first);
    assert(call.args[FN_MAP_ARG_KEY_HASH].fn_ptr_name.first);
    assert(!call.args[FN_MAP_ARG_MAP_OUT].out.isNull());

    auto capacity = call.args[FN_MAP_ARG_CAPACITY].expr;
    auto keq = call.args[FN_MAP_ARG_KEY_EQUAL].fn_ptr_name.second;
    auto khash = call.args[FN_MAP_ARG_KEY_HASH].fn_ptr_name.second;
    auto map_out = call.args[FN_MAP_ARG_MAP_OUT].out;

    auto map_out_addr = kutil::expr_addr_to_obj_addr(map_out);

    if (map_out_addr != map_addr) {
      continue;
    }

    auto capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    auto key_size = get_key_size(bdd, map_addr);

    return map_config_t{capacity_value, key_size, keq, khash};
  }

  assert(false && "Should have found map configuration");

  std::cerr << "Map configuration not found.\n";
  exit(1);
}

vector_config_t get_vector_config(const BDD &bdd, addr_t vector_addr) {
  auto init_nodes = get_call_nodes(bdd.get_init(), {FN_VECTOR_ALLOCATE});

  assert(init_nodes.size() > 0);

  for (auto init_node : init_nodes) {
    auto call_node = cast_node<Call>(init_node);
    auto call = call_node->get_call();

    assert(!call.args[FN_VECTOR_ARG_CAPACITY].expr.isNull());
    assert(!call.args[FN_VECTOR_ARG_ELEM_SIZE].expr.isNull());
    assert(call.args[FN_VECTOR_ARG_INIT_ELEM].fn_ptr_name.first);
    assert(!call.args[FN_VECTOR_ARG_VECTOR_OUT].out.isNull());

    auto capacity = call.args[FN_VECTOR_ARG_CAPACITY].expr;
    auto elem_size = call.args[FN_VECTOR_ARG_ELEM_SIZE].expr;
    auto init_elem = call.args[FN_VECTOR_ARG_INIT_ELEM].fn_ptr_name.second;
    auto vector_out = call.args[FN_VECTOR_ARG_VECTOR_OUT].out;

    auto vector_out_addr = kutil::expr_addr_to_obj_addr(vector_out);

    if (vector_out_addr != vector_addr) {
      continue;
    }

    auto capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    auto elem_size_value = kutil::solver_toolbox.value_from_expr(elem_size);

    return vector_config_t{capacity_value, elem_size_value, init_elem};
  }

  assert(false && "Should have found vector configuration");

  std::cerr << "Vector configuration not found.\n";
  exit(1);
}

sketch_config_t get_sketch_config(const BDD &bdd, addr_t sketch_addr) {
  auto init_nodes = get_call_nodes(bdd.get_init(), {FN_SKETCH_ALLOCATE});

  assert(init_nodes.size() > 0);

  for (auto init_node : init_nodes) {
    auto call_node = cast_node<Call>(init_node);
    auto call = call_node->get_call();

    assert(!call.args[FN_SKETCH_ARG_CAPACITY].expr.isNull());
    assert(!call.args[FN_SKETCH_ARG_THRESHOLD].expr.isNull());
    assert(call.args[FN_SKETCH_ARG_KEY_HASH].fn_ptr_name.first);
    assert(!call.args[FN_SKETCH_ARG_SKETCH_OUT].out.isNull());

    auto capacity = call.args[FN_SKETCH_ARG_CAPACITY].expr;
    auto threshold = call.args[FN_SKETCH_ARG_THRESHOLD].expr;
    auto khash = call.args[FN_SKETCH_ARG_KEY_HASH].fn_ptr_name.second;
    auto sketch_out = call.args[FN_SKETCH_ARG_SKETCH_OUT].out;

    auto sketch_out_addr = kutil::expr_addr_to_obj_addr(sketch_out);

    if (sketch_out_addr != sketch_addr) {
      continue;
    }

    auto capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    auto threshold_value = kutil::solver_toolbox.value_from_expr(threshold);
    auto key_size = get_key_size(bdd, sketch_addr);

    return sketch_config_t{capacity_value, threshold_value, key_size, khash};
  }

  assert(false && "Should have found sketch configuration");

  std::cerr << "Sketch configuration not found.\n";
  exit(1);
}

cht_config_t get_cht_config(const BDD &bdd, addr_t cht_addr) {
  auto init_nodes = get_call_nodes(bdd.get_init(), {FN_CHT_FILL});

  assert(init_nodes.size() > 0);

  for (auto init_node : init_nodes) {
    auto call_node = cast_node<Call>(init_node);
    auto call = call_node->get_call();

    assert(!call.args[FN_CHT_ARG_CAPACITY].expr.isNull());
    assert(!call.args[FN_CHT_ARG_HEIGHT].expr.isNull());

    auto capacity = call.args[FN_CHT_ARG_CAPACITY].expr;
    auto height = call.args[FN_CHT_ARG_HEIGHT].expr;
    auto cht = call.args[FN_CHT_ARG_CHT].expr;

    auto _cht_addr = kutil::expr_addr_to_obj_addr(cht);

    if (_cht_addr != cht_addr) {
      continue;
    }

    auto capacity_value = kutil::solver_toolbox.value_from_expr(capacity);
    auto height_value = kutil::solver_toolbox.value_from_expr(height);

    return cht_config_t{capacity_value, height_value};
  }

  assert(false && "Should have found cht configuration");

  std::cerr << "CHT configuration not found.\n";
  exit(1);
}

} // namespace symbex
} // namespace BDD