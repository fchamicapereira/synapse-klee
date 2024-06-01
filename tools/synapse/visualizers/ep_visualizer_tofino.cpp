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

IGNORE_MODULE(tofino::Ignore)
IGNORE_MODULE(tofino::Then)
IGNORE_MODULE(tofino::Else)

VISIT_BRANCH(tofino::If)
VISIT_BRANCH(tofino::ParserCondition)

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::Forward *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  int dst_device = node->get_dst_device();

  label_builder << "Forward (";
  label_builder << dst_device;
  label_builder << ")";

  std::string label = label_builder.str();
  find_and_replace(label, {{"\n", "\\n"}});

  function_call(ep_node, bdd_node, target, label);
}

SHOW_MODULE_NAME(tofino::Drop)
SHOW_MODULE_NAME(tofino::Broadcast)
SHOW_MODULE_NAME(tofino::ModifyHeader)

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::SimpleTableLookup *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  int tid = node->get_table_id();
  addr_t obj = node->get_obj();

  label_builder << "SimpleTable lookup\n";
  label_builder << "(tid=";
  label_builder << tid;
  label_builder << ", obj=";
  label_builder << obj;
  label_builder << ")";

  std::string label = label_builder.str();
  find_and_replace(label, {{"\n", "\\n"}});

  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::ParserExtraction *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  bytes_t size = node->get_length();

  label_builder << "Parse header (";

  label_builder << size;
  label_builder << "B)";

  auto label = label_builder.str();
  find_and_replace(label, {{"\n", "\\n"}});

  function_call(ep_node, bdd_node, target, label);
}

} // namespace synapse