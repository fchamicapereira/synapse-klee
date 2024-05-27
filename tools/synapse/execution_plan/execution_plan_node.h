#pragma once

#include "call-paths-to-bdd.h"

namespace synapse {

class Module;
class EPVisitor;

typedef uint64_t ep_node_id_t;

enum class EPNodeType { BRANCH, CALL, ROUTE };
enum class EPNodeVisitAction { VISIT_CHILDREN, SKIP_CHILDREN, STOP };

class EPNode {
private:
  ep_node_id_t id;
  std::unique_ptr<const Module> module;
  std::vector<EPNode *> children;
  EPNode *prev;

public:
  EPNode(std::unique_ptr<const Module> &&_module);
  ~EPNode();

  ep_node_id_t get_id() const;
  const Module *get_module() const;
  const std::vector<EPNode *> &get_children() const;
  EPNode *get_prev() const;

  void set_id(ep_node_id_t id);
  void set_children(const std::vector<EPNode *> &children);
  void set_prev(EPNode *prev);

  const EPNode *get_node_by_id(ep_node_id_t target_id) const;
  EPNode *get_mutable_node_by_id(ep_node_id_t target_id);

  bool is_terminal_node() const;
  EPNode *clone(bool recursive = false) const;

  void visit(EPVisitor &visitor) const;
  void visit_nodes(std::function<EPNodeVisitAction(const EPNode *)> fn) const;
  void visit_mutable_nodes(std::function<EPNodeVisitAction(EPNode *)> fn);
};
} // namespace synapse