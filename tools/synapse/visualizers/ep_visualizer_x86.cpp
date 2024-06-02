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

VISIT_BRANCH(x86::If)

SHOW_MODULE_NAME(x86::Then)
SHOW_MODULE_NAME(x86::Else)
SHOW_MODULE_NAME(x86::Forward)
SHOW_MODULE_NAME(x86::Broadcast)
SHOW_MODULE_NAME(x86::Drop)
SHOW_MODULE_NAME(x86::ParseHeader)
SHOW_MODULE_NAME(x86::ModifyHeader)
SHOW_MODULE_NAME(x86::ChecksumUpdate)
SHOW_MODULE_NAME(x86::MapGet)
SHOW_MODULE_NAME(x86::MapPut)
SHOW_MODULE_NAME(x86::MapErase)
SHOW_MODULE_NAME(x86::ExpireItemsSingleMap)
SHOW_MODULE_NAME(x86::ExpireItemsSingleMapIteratively)
SHOW_MODULE_NAME(x86::VectorRead)
SHOW_MODULE_NAME(x86::VectorWrite)
SHOW_MODULE_NAME(x86::DchainAllocateNewIndex)
SHOW_MODULE_NAME(x86::DchainIsIndexAllocated)
SHOW_MODULE_NAME(x86::DchainRejuvenateIndex)
SHOW_MODULE_NAME(x86::DchainFreeIndex)
SHOW_MODULE_NAME(x86::SketchComputeHashes)
SHOW_MODULE_NAME(x86::SketchExpire)
SHOW_MODULE_NAME(x86::SketchFetch)
SHOW_MODULE_NAME(x86::SketchRefresh)
SHOW_MODULE_NAME(x86::SketchTouchBuckets)
SHOW_MODULE_NAME(x86::HashObj)
SHOW_MODULE_NAME(x86::ChtFindBackend)

} // namespace synapse