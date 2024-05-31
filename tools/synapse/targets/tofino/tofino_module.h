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
};

class TofinoModuleGenerator : public ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  TofinoModuleGenerator(ModuleType _type, const std::string &_name)
      : ModuleGenerator(_type, TargetType::Tofino, _name) {}

protected:
  static TNA &get_mutable_tna(EP *ep) {
    Context &ctx = ep->get_mutable_context();
    TofinoContext *tofino_ctx = static_cast<TofinoContext *>(
        ctx.get_mutable_target_context(TargetType::Tofino));
    return tofino_ctx->get_mutable_tna();
  }

  static const TNA &get_tna(const EP *ep) {
    const Context &ctx = ep->get_context();
    const TofinoContext *tofino_ctx = static_cast<const TofinoContext *>(
        ctx.get_target_context(TargetType::Tofino));
    return tofino_ctx->get_tna();
  }
};

} // namespace tofino
} // namespace synapse
