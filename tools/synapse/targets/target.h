#pragma once

#include <memory>
#include <vector>

namespace synapse {

class ModuleGenerator;
class TargetContext;

enum class TargetType {
  x86,
  Tofino,
  TofinoCPU,
};

std::ostream &operator<<(std::ostream &os, TargetType target);

struct Target {
  const TargetType type;
  const std::vector<ModuleGenerator *> module_generators;
  const TargetContext *ctx;

  Target(TargetType _type,
         const std::vector<ModuleGenerator *> &_module_generators,
         const TargetContext *_ctx);

  Target(const Target &other) = delete;
  Target(Target &&other) = delete;

  virtual ~Target();
};

typedef std::vector<const Target *> targets_t;

} // namespace synapse