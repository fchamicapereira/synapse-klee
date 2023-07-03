#pragma once

#include "call-paths-to-bdd.h"
#include "target.h"

namespace synapse {

class ExecutionPlanVisitor;

class ExecutionPlanNode;
typedef std::shared_ptr<ExecutionPlanNode> ExecutionPlanNode_ptr;
typedef std::vector<ExecutionPlanNode_ptr> Branches;

class Module;
typedef std::shared_ptr<Module> Module_ptr;

class ExecutionPlanNode {
  friend class ExecutionPlan;

private:
  Module_ptr module;
  Target_ptr target;  
  Branches next;
  ExecutionPlanNode_ptr prev;
  int id;

  static int counter;

private:
  ExecutionPlanNode(Module_ptr _module, Target_ptr target);
  ExecutionPlanNode(const ExecutionPlanNode *ep_node);

public:
  void set_next(Branches _next);
  void set_next(ExecutionPlanNode_ptr _next);
  void set_prev(ExecutionPlanNode_ptr _prev);

  inline void clear_next() {
    next.clear();
  }

  const Module_ptr &get_module() const;
  void replace_module(Module_ptr _module);

  const Branches &get_next() const;
  ExecutionPlanNode_ptr get_prev() const;

  int get_id() const;
  void set_id(int _id);
  Target_ptr get_target() const;

  bool is_terminal_node() const;

  void replace_next(ExecutionPlanNode_ptr before, ExecutionPlanNode_ptr after);
  void replace_prev(ExecutionPlanNode_ptr _prev);
  void replace_node(BDD::Node_ptr node);

  ExecutionPlanNode_ptr clone(bool recursive = false) const;
  void visit(ExecutionPlanVisitor &visitor) const;

  static ExecutionPlanNode_ptr build(Module_ptr _module, Target_ptr target);
  static ExecutionPlanNode_ptr build(const ExecutionPlanNode *ep_node);
};
} // namespace synapse
