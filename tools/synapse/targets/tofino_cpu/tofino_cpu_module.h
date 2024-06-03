#pragma once

#include "../module.h"
#include "../module_generator.h"

#include "tofino_cpu_context.h"
#include "../tofino/tofino_context.h"

namespace synapse {
namespace tofino_cpu {

class TofinoCPUModule : public Module {
public:
  TofinoCPUModule(ModuleType _type, const std::string &_name,
                  const bdd::Node *node)
      : Module(_type, TargetType::TofinoCPU, _name, node) {}
};

class TofinoCPUModuleGenerator : public ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  TofinoCPUModuleGenerator(ModuleType _type, const std::string &_name)
      : ModuleGenerator(_type, TargetType::TofinoCPU, _name) {}

protected:
  const tofino::TofinoContext *get_tofino_ctx(const EP *ep) const {
    const Context &ctx = ep->get_ctx();
    const tofino::TofinoContext *tofino_ctx =
        ctx.get_target_ctx<tofino::TofinoContext>();
    return tofino_ctx;
  }

  tofino::TofinoContext *get_mutable_tofino_ctx(EP *ep) const {
    Context &ctx = ep->get_mutable_ctx();
    tofino::TofinoContext *tofino_ctx =
        ctx.get_mutable_target_ctx<tofino::TofinoContext>();
    return tofino_ctx;
  }
};

} // namespace tofino_cpu
} // namespace synapse
