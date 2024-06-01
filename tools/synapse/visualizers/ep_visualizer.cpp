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

static std::unordered_map<TargetType, std::string> node_colors = {
    {TargetType::Tofino, "cornflowerblue"},
    {TargetType::TofinoCPU, "firebrick2"},
    {TargetType::x86, "orange"},
};

static std::unordered_set<ModuleType> modules_to_ignore = {
    ModuleType::Tofino_Ignore, ModuleType::Tofino_Then, ModuleType::Tofino_Else,
    ModuleType::x86_Then,      ModuleType::x86_Else,
};

static bool should_ignore_node(const EPNode *node) {
  const Module *module = node->get_module();
  ModuleType type = module->get_type();
  return modules_to_ignore.find(type) != modules_to_ignore.end();
}

EPVisualizer::EPVisualizer() {}

void EPVisualizer::log(const EPNode *ep_node) const {
  // Don't log anything.
}

void EPVisualizer::function_call(const EPNode *ep_node, const bdd::Node *node,
                                 TargetType target, const std::string &label) {
  assert(node_colors.find(target) != node_colors.end());
  ss << "[label=\"";

  ss << "[";
  ss << ep_node->get_id();
  ss << "] ";

  ss << label << "\", ";
  ss << "color=" << node_colors[target] << "];";
  ss << "\n";
}

void EPVisualizer::branch(const EPNode *ep_node, const bdd::Node *node,
                          TargetType target, const std::string &label) {
  assert(node_colors.find(target) != node_colors.end());
  ss << "[shape=Mdiamond, label=\"";

  ss << "[";
  ss << ep_node->get_id();
  ss << "] ";

  ss << label << "\", ";
  ss << "color=" << node_colors[target] << "];";
  ss << "\n";
}

void EPVisualizer::visualize(const EP *ep, bool interrupt) {
  EPVisualizer visualizer;
  visualizer.visit(ep);
  visualizer.show(interrupt);
}

void EPVisualizer::visit(const EP *ep) {
  ss << "digraph EP {\n";
  ss << "layout=\"dot\";";
  ss << "node [shape=record,style=filled];\n";

  EPVisitor::visit(ep);

  ss << "}\n";
  ss.flush();
}

void EPVisualizer::visit(const EP *ep, const EPNode *node) {
  if (should_ignore_node(node)) {
    EPVisitor::visit(ep, node);
    return;
  }

  ss << node->get_id() << " ";
  EPVisitor::visit(ep, node);

  const std::vector<EPNode *> &children = node->get_children();
  for (const EPNode *child : children) {
    while (child && should_ignore_node(child)) {
      assert(child->get_children().size() <= 1);
      if (!child->get_children().empty()) {
        child = child->get_children().front();
      } else {
        child = nullptr;
      }
    }

    if (!child) {
      continue;
    }

    ss << node->get_id() << " -> " << child->get_id() << ";"
       << "\n";
  }
}

} // namespace synapse