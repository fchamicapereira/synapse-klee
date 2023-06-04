#pragma once

#include "../../../../log.h"
#include "../../../execution_plan.h"
#include "../../../modules/tofino/data_structures/data_structures.h"
#include "../synthesizer.h"
#include "constants.h"
#include "pipeline/pipeline.h"
#include "transpiler.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

namespace target = synapse::targets::tofino;

class TofinoGenerator : public Synthesizer {
  friend Transpiler;

private:
  Ingress ingress;
  Transpiler transpiler;

public:
  TofinoGenerator()
      : Synthesizer(GET_BOILERPLATE_PATH(TOFINO_BOILERPLATE_FILE)),
        ingress(get_indentation_level(MARKER_INGRESS_HEADERS_DEF),
                get_indentation_level(MARKER_INGRESS_HEADERS_DECL),
                get_indentation_level(MARKER_INGRESS_METADATA),
                get_indentation_level(MARKER_INGRESS_PARSER),
                get_indentation_level(MARKER_INGRESS_STATE),
                get_indentation_level(MARKER_INGRESS_APPLY),
                get_indentation_level(MARKER_CPU_HEADER_FIELDS)),
        transpiler(*this) {}

  virtual void generate(ExecutionPlan &target_ep) override { visit(target_ep); }

  void visit(ExecutionPlan ep) override;
  void visit(const ExecutionPlanNode *ep_node) override;

  void visit(const ExecutionPlanNode *ep_node, const target::If *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Then *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Else *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Forward *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::ParseCustomHeader *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::ModifyCustomHeader *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::ParserCondition *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::IPv4TCPUDPChecksumsUpdate *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Drop *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::Ignore *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::SendToController *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::TableModule *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::TableLookup *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::TableIsAllocated *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::TableRejuvenation *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::IntegerAllocatorAllocate *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::IntegerAllocatorRejuvenate *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::IntegerAllocatorQuery *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::CounterRead *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::CounterIncrement *node) override;
  void visit(const ExecutionPlanNode *ep_node,
             const target::HashObj *node) override;

  std::string transpile(klee::ref<klee::Expr> expr);

  variable_query_t search_variable(const BDD::symbol_t &symbol) const;
  variable_query_t search_variable(const std::string &symbol) const;
  variable_query_t search_variable(klee::ref<klee::Expr> expr) const;

private:
  void build_cpu_header(const ExecutionPlan &ep);
  void allocate_state(const ExecutionPlan &ep);
  void allocate_table(const target::Table *table);
  void allocate_int_allocator(const target::IntegerAllocator *int_allocator);
  void allocate_counter(const target::Counter *counter);
  void
  visit_if_multiple_conditions(std::vector<klee::ref<klee::Expr>> conditions);
  void visit_if_simple_condition(klee::ref<klee::Expr> condition);
};

} // namespace tofino
} // namespace synthesizer
} // namespace synapse
