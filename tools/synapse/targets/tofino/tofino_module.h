#pragma once

#include "../module.h"
#include "../module_generator.h"
#include "tofino_context.h"

namespace synapse {
namespace tofino {

class TofinoModule : public Module {
public:
  TofinoModule(ModuleType _type, const std::string &_name,
               const bdd::Node *node)
      : Module(_type, TargetType::Tofino, _name, node) {}

  TofinoModule(ModuleType _type, TargetType _next_type,
               const std::string &_name, const bdd::Node *node)
      : Module(_type, TargetType::Tofino, _next_type, _name, node) {}
};

class TofinoModuleGenerator : public ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  TofinoModuleGenerator(ModuleType _type, const std::string &_name)
      : ModuleGenerator(_type, TargetType::Tofino, _name) {}

protected:
  static TofinoContext *get_mutable_tofino_ctx(EP *ep) {
    Context &ctx = ep->get_mutable_ctx();
    return ctx.get_mutable_target_ctx<TofinoContext>();
  }

  static const TofinoContext *get_tofino_ctx(const EP *ep) {
    const Context &ctx = ep->get_ctx();
    return ctx.get_target_ctx<TofinoContext>();
  }
};

} // namespace tofino
} // namespace synapse
