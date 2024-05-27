#pragma once

#include "../module.h"
#include "../module_generator.h"
#include "data_structures/data_structures.h"
#include "x86_context.h"

namespace synapse {
namespace x86 {

class x86Module : public Module {
public:
  x86Module(ModuleType _type, const char *_name, const bdd::Node *node)
      : Module(_type, TargetType::x86, _name, node) {}

protected:
  void save_map(const EP &ep, addr_t addr) {
    auto ctx = ep.get_context<x86Context>(TargetType::x86);
    auto saved = ctx->has_map_config(addr);
    if (!saved) {
      const bdd::BDD *bdd = ep.get_bdd();
      auto cfg = bdd::get_map_config(*bdd, addr);
      ctx->save_map_config(addr, cfg);
    }
  }

  void save_vector(const EP &ep, addr_t addr) {
    auto ctx = ep.get_context<x86Context>(TargetType::x86);
    auto saved = ctx->has_vector_config(addr);
    if (!saved) {
      const bdd::BDD *bdd = ep.get_bdd();
      auto cfg = bdd::get_vector_config(*bdd, addr);
      ctx->save_vector_config(addr, cfg);
    }
  }

  void save_dchain(const EP &ep, addr_t addr) {
    auto ctx = ep.get_context<x86Context>(TargetType::x86);
    auto saved = ctx->has_dchain_config(addr);
    if (!saved) {
      const bdd::BDD *bdd = ep.get_bdd();
      auto cfg = bdd::get_dchain_config(*bdd, addr);
      ctx->save_dchain_config(addr, cfg);
    }
  }

  void save_sketch(const EP &ep, addr_t addr) {
    auto ctx = ep.get_context<x86Context>(TargetType::x86);
    auto saved = ctx->has_sketch_config(addr);
    if (!saved) {
      const bdd::BDD *bdd = ep.get_bdd();
      auto cfg = bdd::get_sketch_config(*bdd, addr);
      ctx->save_sketch_config(addr, cfg);
    }
  }

  void save_cht(const EP &ep, addr_t addr) {
    auto ctx = ep.get_context<x86Context>(TargetType::x86);
    auto saved = ctx->has_cht_config(addr);
    if (!saved) {
      const bdd::BDD *bdd = ep.get_bdd();
      auto cfg = bdd::get_cht_config(*bdd, addr);
      ctx->save_cht_config(addr, cfg);
    }
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const = 0;
  virtual bool equals(const Module *other) const = 0;
};

class x86ModuleGenerator : public ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  x86ModuleGenerator(ModuleType _type)
      : ModuleGenerator(_type, TargetType::x86) {}
};

} // namespace x86
} // namespace synapse
