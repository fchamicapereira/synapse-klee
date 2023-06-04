#pragma once

#include <sstream>
#include <vector>

#include "../../../../log.h"
#include "../../../execution_plan.h"
#include "../code_builder.h"
#include "../synthesizer.h"
#include "../util.h"
#include "constants.h"
#include "transpiler.h"

#include "domain/headers.h"
#include "domain/stack.h"
#include "domain/variable.h"

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

namespace target = synapse::targets::x86_tofino;

class x86TofinoGenerator : public Synthesizer {
  friend Transpiler;

private:
  CodeBuilder state_decl_builder;
  CodeBuilder state_init_builder;
  CodeBuilder nf_process_builder;
  CodeBuilder cpu_hdr_fields_builder;

  Transpiler transpiler;

  Headers headers;
  stack_t vars;
  PendingIfs pending_ifs;

public:
  x86TofinoGenerator()
      : Synthesizer(GET_BOILERPLATE_PATH(BOILERPLATE_FILE)),
        state_decl_builder(get_indentation_level(MARKER_STATE_DECL)),
        state_init_builder(get_indentation_level(MARKER_STATE_INIT)),
        nf_process_builder(get_indentation_level(MARKER_NF_PROCESS)),
        cpu_hdr_fields_builder(get_indentation_level(MARKER_CPU_HEADER)),
        transpiler(*this), pending_ifs(nf_process_builder) {}

  std::string transpile(klee::ref<klee::Expr> expr);
  virtual void generate(ExecutionPlan &target_ep) override { visit(target_ep); }

  void init_state(ExecutionPlan ep);

  variable_query_t search_variable(std::string symbol) const;
  variable_query_t search_variable(klee::ref<klee::Expr> expr) const;

  void visit(ExecutionPlan ep) override;
  void visit(const ExecutionPlanNode *ep_node) override;

  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketParseCPU *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Drop *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::ForwardThroughTofino *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketParseEthernet *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketModifyEthernet *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketParseIPv4 *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketModifyIPv4 *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketParseIPv4Options *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketModifyIPv4Options *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketParseTCPUDP *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketModifyTCPUDP *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::PacketModifyChecksums *node) override;
  void visit(const ExecutionPlanNode *ep_node, const target::If *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Then *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Else *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::MapGet *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::MapPut *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::EtherAddrHash *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::DchainAllocateNewIndex *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::DchainIsIndexAllocated *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::DchainRejuvenateIndex *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::HashObj *node) override;
};

} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse