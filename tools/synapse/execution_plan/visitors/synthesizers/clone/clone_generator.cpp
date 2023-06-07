#include "clone_generator.h"

namespace synapse {
namespace synthesizer {
namespace clone {


void CloneGenerator::visit(ExecutionPlan ep) {

}

void CloneGenerator::visit(const ExecutionPlanNode *ep_node) {
}

void CloneGenerator::visit(const ExecutionPlanNode *ep_node,
                         const target::If *node) {
  // empty
}

void CloneGenerator::visit(const ExecutionPlanNode *ep_node,
                         const target::Then *node) {
  // empty
}

void CloneGenerator::visit(const ExecutionPlanNode *ep_node,
                         const target::Else *node) {
  // empty
}

void CloneGenerator::visit(const ExecutionPlanNode *ep_node,
                         const target::Drop *node) {
  // empty
}

} // namespace clone
} // namespace synthesizer
} // namespace synapse
