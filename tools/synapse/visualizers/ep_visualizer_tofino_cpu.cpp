#include "ep_visualizer.h"

#include "../log.h"
#include "../execution_plan/execution_plan.h"
#include "../targets/targets.h"

#include <ctime>
#include <fstream>
#include <limits>
#include <math.h>
#include <unistd.h>

#define SHOW_MODULE_NAME(M)                                                    \
  void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,                \
                           const M *node) {                                    \
    function_call(ep_node, node->get_node(), node->get_target(),               \
                  node->get_name());                                           \
  }

#define VISIT_BRANCH(M)                                                        \
  void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,                \
                           const M *node) {                                    \
    branch(ep_node, node->get_node(), node->get_target(), node->get_name());   \
  }

#define IGNORE_MODULE(M)                                                       \
  void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,                \
                           const M *node) {}

namespace synapse {

IGNORE_MODULE(tofino_cpu::Ignore)

VISIT_BRANCH(tofino_cpu::If)

SHOW_MODULE_NAME(tofino_cpu::ParseHeader)
SHOW_MODULE_NAME(tofino_cpu::ModifyHeader)

SHOW_MODULE_NAME(tofino_cpu::Then)
SHOW_MODULE_NAME(tofino_cpu::Else)
SHOW_MODULE_NAME(tofino_cpu::Forward)
SHOW_MODULE_NAME(tofino_cpu::Broadcast)
SHOW_MODULE_NAME(tofino_cpu::Drop)
SHOW_MODULE_NAME(tofino_cpu::SimpleTableLookup)
SHOW_MODULE_NAME(tofino_cpu::SimpleTableUpdate)
SHOW_MODULE_NAME(tofino_cpu::DchainAllocateNewIndex)
SHOW_MODULE_NAME(tofino_cpu::VectorRead)
SHOW_MODULE_NAME(tofino_cpu::VectorWrite)

} // namespace synapse