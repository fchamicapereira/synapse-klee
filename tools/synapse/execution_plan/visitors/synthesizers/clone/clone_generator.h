#pragma once

#include <sstream>
#include <vector>

#include "../../../../log.h"
#include "../../../execution_plan.h"
#include "../code_builder.h"
#include "../synthesizer.h"
#include "../util.h"


namespace synapse {
namespace synthesizer {
namespace clone {

namespace target = synapse::targets::clone;

class CloneGenerator : public Synthesizer {
public:
  CloneGenerator(): Synthesizer(GET_BOILERPLATE_PATH("empty.txt")) {}

  std::string transpile(klee::ref<klee::Expr> expr);
  virtual void generate(ExecutionPlan &target_ep) override { visit(target_ep); }

  void init_state(ExecutionPlan ep);

  void visit(ExecutionPlan ep) override;
  void visit(const ExecutionPlanNode *ep_node) override;

  void visit(const ExecutionPlanNode *ep_node,
             const target::If *node) override;
  void visit(const ExecutionPlanNode *ep_node,
              const target::Then *node) override;
  void visit(const ExecutionPlanNode *ep_node,
              const target::Else *node) override;
  void visit(const ExecutionPlanNode *ep_node,
              const target::Drop *node) override;
};

} // namespace clone
} // namespace synthesizer
} // namespace synapse
