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

  virtual std::unordered_set<DS_ID> get_generated_ds() const { return {}; }
};

class TofinoModuleGenerator : public ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  TofinoModuleGenerator(ModuleType _type, const std::string &_name)
      : ModuleGenerator(_type, TargetType::Tofino, _name) {}

protected:
  TofinoContext *get_mutable_tofino_ctx(EP *ep) const {
    Context &ctx = ep->get_mutable_ctx();
    return ctx.get_mutable_target_ctx<TofinoContext>();
  }

  const TofinoContext *get_tofino_ctx(const EP *ep) const {
    const Context &ctx = ep->get_ctx();
    return ctx.get_target_ctx<TofinoContext>();
  }

  TNA &get_mutable_tna(EP *ep) const {
    TofinoContext *ctx = get_mutable_tofino_ctx(ep);
    return ctx->get_mutable_tna();
  }

  const TNA &get_tna(const EP *ep) const {
    const TofinoContext *ctx = get_tofino_ctx(ep);
    return ctx->get_tna();
  }
};

} // namespace tofino
} // namespace synapse
