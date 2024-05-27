#pragma once

#include <functional>
#include <iostream>
#include <unordered_set>
#include <vector>

#include "load-call-paths.h"

namespace bdd {

class BDDVisitor;
class NodeManager;

typedef uint64_t node_id_t;
typedef std::unordered_set<node_id_t> nodes_t;

enum class NodeType { BRANCH, CALL, ROUTE };
enum class NodeVisitAction { VISIT_CHILDREN, SKIP_CHILDREN, STOP };

class Node {
protected:
  node_id_t id;
  NodeType type;

  Node *next;
  Node *prev;

  klee::ConstraintManager constraints;

public:
  Node(node_id_t _id, NodeType _type, klee::ConstraintManager _constraints)
      : id(_id), type(_type), next(nullptr), prev(nullptr),
        constraints(_constraints) {}

  Node(node_id_t _id, NodeType _type, Node *_next, Node *_prev,
       klee::ConstraintManager _constraints)
      : id(_id), type(_type), next(_next), prev(_prev),
        constraints(_constraints) {}

  const Node *get_next() const { return next; }
  const Node *get_prev() const { return prev; }

  void set_next(Node *_next) { next = _next; }
  void set_prev(Node *_prev) { prev = _prev; }

  NodeType get_type() const { return type; }
  node_id_t get_id() const { return id; }

  const klee::ConstraintManager &get_constraints() const { return constraints; }

  Node *get_mutable_next() { return next; }
  Node *get_mutable_prev() { return prev; }

  const Node *get_node_by_id(node_id_t _id) const;
  Node *get_mutable_node_by_id(node_id_t _id);

  void visit_nodes(std::function<NodeVisitAction(const Node *)> fn) const;
  void visit_mutable_nodes(std::function<NodeVisitAction(Node *)> fn);

  void recursive_update_ids(node_id_t &new_id);
  void recursive_translate_symbol(const symbol_t &old_symbol,
                                  const symbol_t &new_symbol);
  std::string recursive_dump(int lvl = 0) const;
  void recursive_add_constraint(klee::ref<klee::Expr> constraint);

  bool is_reachable_by_node(node_id_t id) const;
  std::string hash(bool recursive) const;
  size_t count_children(bool recursive) const;
  size_t count_code_paths() const;

  virtual std::vector<node_id_t> get_terminating_node_ids() const;
  virtual Node *clone(NodeManager &manager, bool recursive = false) const = 0;
  virtual void visit(BDDVisitor &visitor) const = 0;
  virtual std::string dump(bool one_liner = false) const = 0;

  virtual ~Node() = default;
};

} // namespace bdd