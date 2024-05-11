#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include <optional>

namespace bdd {

std::optional<addr_t> get_obj_from_call(const Call *call);

struct dchain_config_t {
  uint64_t index_range;
};

struct map_config_t {
  uint64_t capacity;
  bits_t key_size;
};

struct vector_config_t {
  uint64_t capacity;
  bits_t elem_size;
};

struct sketch_config_t {
  uint64_t capacity;
  uint64_t threshold;
  bits_t key_size;
};

struct cht_config_t {
  uint64_t capacity;
  uint64_t height;
};

dchain_config_t get_dchain_config(const BDD &bdd, addr_t dchain_addr);
map_config_t get_map_config(const BDD &bdd, addr_t map_addr);
vector_config_t get_vector_config(const BDD &bdd, addr_t vector_addr);
sketch_config_t get_sketch_config(const BDD &bdd, addr_t sketch_addr);
cht_config_t get_cht_config(const BDD &bdd, addr_t cht_addr);

} // namespace bdd