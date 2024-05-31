#include "ep_visualizer.h"

#include "../log.h"
#include "../search_space.h"
#include "../execution_plan/execution_plan.h"
#include "../targets/targets.h"

#include <ctime>
#include <fstream>
#include <limits>
#include <math.h>
#include <unistd.h>

#define SHOW_MODULE_NAME(M)                                                    \
  void EPVisualizer::visit(const EPNode *ep_node, const M *node) {             \
    function_call(ep_node, node->get_node(), node->get_target(),               \
                  node->get_name());                                           \
  }

#define VISIT_BRANCH(M)                                                        \
  void EPVisualizer::visit(const EPNode *ep_node, const M *node) {             \
    branch(ep_node, node->get_node(), node->get_target(), node->get_name());   \
  }

#define IGNORE_MODULE(M)                                                       \
  void EPVisualizer::visit(const EPNode *ep_node, const M *node) {}

namespace synapse {

static std::unordered_map<TargetType, std::string> node_colors = {
    {TargetType::Tofino, "cornflowerblue"},
    {TargetType::TofinoCPU, "firebrick2"},
    {TargetType::x86, "orange"},
};

static std::unordered_set<ModuleType> modules_to_ignore = {
    ModuleType::Tofino_Ignore,
};

static bool should_ignore_node(const EPNode *node) {
  const Module *module = node->get_module();
  ModuleType type = module->get_type();
  return modules_to_ignore.find(type) != modules_to_ignore.end();
}

EPVisualizer::EPVisualizer() {}

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

void EPVisualizer::visit(const EPNode *node) {
  if (should_ignore_node(node)) {
    EPVisitor::visit(node);
    return;
  }

  ss << node->get_id() << " ";
  EPVisitor::visit(node);

  const std::vector<EPNode *> &children = node->get_children();
  for (const EPNode *child : children) {
    while (child && should_ignore_node(child)) {
      assert(child->get_children().size() == 1);
      child = child->get_children().front();
    }

    if (!child) {
      continue;
    }

    ss << node->get_id() << " -> " << child->get_id() << ";"
       << "\n";
  }
}

/********************************************
 *
 *                  Tofino
 *
 ********************************************/

IGNORE_MODULE(tofino::Ignore)
VISIT_BRANCH(tofino::If)
VISIT_BRANCH(tofino::IfHeaderValid)
SHOW_MODULE_NAME(tofino::Then)
SHOW_MODULE_NAME(tofino::Else)
SHOW_MODULE_NAME(tofino::Forward)
SHOW_MODULE_NAME(tofino::Drop)
SHOW_MODULE_NAME(tofino::Broadcast)
SHOW_MODULE_NAME(tofino::ModifyHeader)

void EPVisualizer::visit(const EPNode *ep_node,
                         const tofino::ParseHeader *node) {
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

// SHOW_MODULE_NAME(tofino::IPv4TCPUDPChecksumsUpdate)

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino::TableModule *node) {
//   std::stringstream label_builder;

//   auto bdd_node = node->get_node();
//   auto target = node->get_target();
//   auto table = node->get_table();

//   assert(table);

//   table->dump(label_builder);

//   auto label = label_builder.str();
//   find_and_replace(label, {{"\n", "\\l"}});
//   function_call(ep_node, bdd_node, target, label);
// }

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino::TableLookup *node) {
//   visit(ep_node, static_cast<const tofino::TableModule *>(node));
// }

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino::TableRejuvenation *node) {
//   visit(ep_node, static_cast<const tofino::TableModule *>(node));
// }

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino::TableIsAllocated *node) {
//   visit(ep_node, static_cast<const tofino::TableModule *>(node));
// }

// SHOW_MODULE_NAME(tofino::IntegerAllocatorAllocate)
// SHOW_MODULE_NAME(tofino::IntegerAllocatorRejuvenate)
// SHOW_MODULE_NAME(tofino::IntegerAllocatorQuery)

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino::SendToController *node) {
//   std::stringstream label_builder;

//   auto bdd_node = node->get_node();
//   auto target = node->get_target();
//   auto cpu_path = node->get_cpu_code_path();
//   auto state = node->get_dataplane_state();

//   label_builder << "Send To Controller\n";
//   label_builder << "\tPath ID: " << cpu_path << "\n";
//   label_builder << "\tDataplane state:\n";
//   for (auto s : state) {
//     label_builder << "\t" << s.array->name << "\n";
//   }

//   auto label = label_builder.str();
//   find_and_replace(label, {{"\n", "\\l"}});

//   function_call(ep_node, bdd_node, target, label);
// }

// SHOW_MODULE_NAME(tofino::SetupExpirationNotifications)
// SHOW_MODULE_NAME(tofino::CounterRead)
// SHOW_MODULE_NAME(tofino::CounterIncrement)
// SHOW_MODULE_NAME(tofino::HashObj)

/********************************************
 *
 *               Tofino CPU
 *
 ********************************************/

// VISIT_BRANCH(tofino_cpu::If)
// SHOW_MODULE_NAME(tofino_cpu::Then)
// SHOW_MODULE_NAME(tofino_cpu::Else)
// SHOW_MODULE_NAME(tofino_cpu::Ignore)
// SHOW_MODULE_NAME(tofino_cpu::PacketParseEthernet)
// SHOW_MODULE_NAME(tofino_cpu::PacketModifyEthernet)
// SHOW_MODULE_NAME(tofino_cpu::ForwardThroughTofino)

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino_cpu::PacketParseCPU *node) {
//   std::stringstream label_builder;

//   auto bdd_node = node->get_node();
//   auto target = node->get_target();
//   auto state = node->get_dataplane_state();

//   label_builder << "Parse CPU Header\n";
//   label_builder << "  Dataplane state:\n";
//   for (auto s : state) {
//     label_builder << "    [";
//     label_builder << s.array->name;
//     label_builder << "]\n";
//   }

//   auto label = label_builder.str();
//   find_and_replace(label, {{"\n", "\\l"}});

//   function_call(ep_node, bdd_node, target, label);
// }

// SHOW_MODULE_NAME(tofino_cpu::PacketParseIPv4)
// SHOW_MODULE_NAME(tofino_cpu::PacketModifyIPv4)
// SHOW_MODULE_NAME(tofino_cpu::PacketParseIPv4Options)
// SHOW_MODULE_NAME(tofino_cpu::PacketModifyIPv4Options)
// SHOW_MODULE_NAME(tofino_cpu::PacketParseTCPUDP)
// SHOW_MODULE_NAME(tofino_cpu::PacketModifyTCPUDP)
// SHOW_MODULE_NAME(tofino_cpu::PacketModifyChecksums)
// SHOW_MODULE_NAME(tofino_cpu::Drop)
// SHOW_MODULE_NAME(tofino_cpu::MapGet)
// SHOW_MODULE_NAME(tofino_cpu::MapPut)
// SHOW_MODULE_NAME(tofino_cpu::MapErase)
// SHOW_MODULE_NAME(tofino_cpu::DchainAllocateNewIndex)
// SHOW_MODULE_NAME(tofino_cpu::DchainIsIndexAllocated)
// SHOW_MODULE_NAME(tofino_cpu::DchainRejuvenateIndex)
// SHOW_MODULE_NAME(tofino_cpu::DchainFreeIndex)
// SHOW_MODULE_NAME(tofino_cpu::PacketParseTCP)
// SHOW_MODULE_NAME(tofino_cpu::PacketModifyTCP)
// SHOW_MODULE_NAME(tofino_cpu::PacketParseUDP)
// SHOW_MODULE_NAME(tofino_cpu::PacketModifyUDP)
// SHOW_MODULE_NAME(tofino_cpu::HashObj)

/********************************************
 *
 *                     x86
 *
 ********************************************/

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