#pragma once

#include <klee/Expr.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "data_structures/data_structures.h"

#define HAS_CONFIG(T)                                                          \
  bool has_##T##_config(addr_t addr) const {                                   \
    return T##_configs.find(addr) != T##_configs.end();                        \
  }

#define SAVE_CONFIG(T)                                                         \
  void save_##T##_config(addr_t addr, bdd::T##_config_t cfg) {                 \
    assert(!has_##T##_config(addr));                                           \
    T##_configs.insert({addr, cfg});                                           \
  }

#define GET_CONFIG(T)                                                          \
  const std::unordered_map<addr_t, bdd::T##_config_t> &get_##T##_configs() {   \
    return T##_configs;                                                        \
  }

namespace synapse {
namespace x86 {

class x86Context : public TargetContext {
public:
private:
  std::unordered_map<addr_t, bdd::map_config_t> map_configs;
  std::unordered_map<addr_t, bdd::vector_config_t> vector_configs;
  std::unordered_map<addr_t, bdd::dchain_config_t> dchain_configs;
  std::unordered_map<addr_t, bdd::sketch_config_t> sketch_configs;
  std::unordered_map<addr_t, bdd::cht_config_t> cht_configs;

public:
  x86Context() {}

  x86Context(const x86Context &other)
      : map_configs(other.map_configs), vector_configs(other.vector_configs),
        dchain_configs(other.dchain_configs),
        sketch_configs(other.sketch_configs), cht_configs(other.cht_configs) {}

  x86Context(x86Context &&other)
      : map_configs(std::move(other.map_configs)),
        vector_configs(std::move(other.vector_configs)),
        dchain_configs(std::move(other.dchain_configs)),
        sketch_configs(std::move(other.sketch_configs)),
        cht_configs(std::move(other.cht_configs)) {}

  HAS_CONFIG(map)
  HAS_CONFIG(vector)
  HAS_CONFIG(dchain)
  HAS_CONFIG(sketch)
  HAS_CONFIG(cht)

  SAVE_CONFIG(map)
  SAVE_CONFIG(vector)
  SAVE_CONFIG(dchain)
  SAVE_CONFIG(sketch)
  SAVE_CONFIG(cht)

  GET_CONFIG(map)
  GET_CONFIG(vector)
  GET_CONFIG(dchain)
  GET_CONFIG(sketch)
  GET_CONFIG(cht)

  virtual TargetContext *clone() const override {
    return new x86Context(*this);
  }
};

} // namespace x86
} // namespace synapse