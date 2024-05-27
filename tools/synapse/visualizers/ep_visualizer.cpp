#include "ep_visualizer.h"

#include "../log.h"
#include "../search_space.h"
#include "../execution_plan/execution_plan.h"
#include "../targets/modules.h"

#include <ctime>
#include <fstream>
#include <limits>
#include <math.h>
#include <unistd.h>

#define DEFAULT_VISIT_PRINT_MODULE_NAME(M)                                     \
  void EPVisualizer::visit(const EPNode *ep_node, const M *node) {             \
    function_call(ep_node, node->get_node(), node->get_target(),               \
                  node->get_name());                                           \
  }

#define DEFAULT_BRANCH_VISIT_PRINT_MODULE_NAME(M)                              \
  void EPVisualizer::visit(const EPNode *ep_node, const M *node) {             \
    branch(ep_node, node->get_node(), node->get_target(), node->get_name());   \
  }

namespace synapse {

static std::unordered_map<TargetType, std::string> node_colors = {
    {TargetType::Tofino, "cornflowerblue"},
    {TargetType::TofinoCPU, "firebrick2"},
    {TargetType::x86, "cadetblue1"},
};

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

void EPVisualizer::visualize(const EP &ep, bool interrupt) {
  EPVisualizer visualizer;
  visualizer.visit(ep);
  visualizer.show(interrupt);
}

void EPVisualizer::visit(EP ep) {
  ss << "digraph EP {\n";
  ss << "layout=\"dot\";";
  ss << "node [shape=record,style=filled];\n";

  EPVisitor::visit(ep);

  ss << "}\n";
  ss.flush();
}

void EPVisualizer::visit(const EPNode *node) {
  ep_node_id_t id = node->get_id();

  ss << id << " ";
  EPVisitor::visit(node);

  const std::vector<EPNode *> &children = node->get_children();
  for (const EPNode *child : children) {
    ss << id << " -> " << child->get_id() << ";"
       << "\n";
  }
}

/********************************************
 *
 *                  Tofino
 *
 ********************************************/

// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::Ignore)

// void EPVisualizer::visit(const EPNode *ep_node, const tofino::If *node) {
//   std::stringstream label_builder;

//   auto bdd_node = node->get_node();
//   auto target = node->get_target();
//   auto conditions = node->get_conditions();

//   for (auto i = 0u; i < conditions.size(); i++) {
//     auto condition = conditions[i];

//     if (i > 0) {
//       label_builder << "\n&& ";
//     }

//     label_builder << kutil::pretty_print_expr(condition) << "\n";
//   }

//   auto label = label_builder.str();
//   find_and_replace(label, {{"\n", "\\n"}});

//   branch(ep_node, bdd_node, target, label);
// }

// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::Then)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::Else)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::Forward)

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino::ParseCustomHeader *node) {
//   std::stringstream label_builder;

//   auto bdd_node = node->get_node();
//   auto target = node->get_target();
//   auto size = node->get_size();

//   label_builder << "Parse header [";
//   label_builder << size;
//   label_builder << " bits (";
//   label_builder << size / 8;
//   label_builder << " bytes)]";

//   auto label = label_builder.str();
//   find_and_replace(label, {{"\n", "\\n"}});

//   function_call(ep_node, bdd_node, target, label);
// }

// void EPVisualizer::visit(const EPNode *ep_node,
//                          const tofino::ParserCondition *node) {
//   std::stringstream label_builder;

//   auto bdd_node = node->get_node();
//   auto target = node->get_target();
//   auto condition = node->get_condition();
//   auto apply_is_valid = node->get_apply_is_valid();

//   label_builder << "Parser condition ";
//   label_builder << " [apply: " << apply_is_valid << "]";
//   label_builder << "\n";
//   label_builder << kutil::pretty_print_expr(condition);

//   auto label = label_builder.str();
//   find_and_replace(label, {{"\n", "\\n"}});

//   branch(ep_node, bdd_node, target, label);
// }

// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::ModifyCustomHeader)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::IPv4TCPUDPChecksumsUpdate)

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

// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::IntegerAllocatorAllocate)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::IntegerAllocatorRejuvenate)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::IntegerAllocatorQuery)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::Drop)

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

// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::SetupExpirationNotifications)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::CounterRead)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::CounterIncrement)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino::HashObj)

/********************************************
 *
 *                x86 Tofino
 *
 ********************************************/

// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::Ignore)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketParseEthernet)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketModifyEthernet)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::ForwardThroughTofino)

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

// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketParseIPv4)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketModifyIPv4)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketParseIPv4Options)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketModifyIPv4Options)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketParseTCPUDP)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketModifyTCPUDP)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketModifyChecksums)
// DEFAULT_BRANCH_VISIT_PRINT_MODULE_NAME(tofino_cpu::If)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::Then)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::Else)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::Drop)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::MapGet)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::MapPut)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::MapErase)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::DchainAllocateNewIndex)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::DchainIsIndexAllocated)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::DchainRejuvenateIndex)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::DchainFreeIndex)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketParseTCP)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketModifyTCP)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketParseUDP)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::PacketModifyUDP)
// DEFAULT_VISIT_PRINT_MODULE_NAME(tofino_cpu::HashObj)

/********************************************
 *
 *                     x86
 *
 ********************************************/

// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::MapGet)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::CurrentTime)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::PacketBorrowNextChunk)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::PacketReturnChunk)
// DEFAULT_BRANCH_VISIT_PRINT_MODULE_NAME(x86::If)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::Then)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::Else)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::Forward)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::Broadcast)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::Drop)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::ExpireItemsSingleMap)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::ExpireItemsSingleMapIteratively)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::DchainRejuvenateIndex)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::VectorBorrow)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::VectorReturn)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::DchainAllocateNewIndex)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::MapPut)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::SetIpv4UdpTcpChecksum)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::DchainIsIndexAllocated)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::SketchComputeHashes)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::SketchExpire)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::SketchFetch)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::SketchRefresh)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::SketchTouchBuckets)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::MapErase)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::DchainFreeIndex)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::LoadBalancedFlowHash)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::ChtFindBackend)
// DEFAULT_VISIT_PRINT_MODULE_NAME(x86::HashObj)

} // namespace synapse