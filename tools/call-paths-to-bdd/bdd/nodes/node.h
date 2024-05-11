#pragma once

#include <iostream>
#include <unordered_set>
#include <vector>

#include "load-call-paths.h"

namespace bdd {

class BDDVisitor;
class NodeManager;

typedef uint64_t node_id_t;

class Node {
public:
  enum NodeType { BRANCH, CALL, ROUTE };

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

  // Get generated symbols, but no further than any of these node IDs
  symbols_t get_generated_symbols(
      const std::unordered_set<node_id_t> &furthest_back_nodes) const;

  symbols_t get_generated_symbols() const;
  bool is_reachable_by_node(node_id_t id) const;
  std::string hash(bool recursive = false) const;
  unsigned count_children(bool recursive = true) const;
  unsigned count_code_paths() const;

  virtual std::vector<node_id_t> get_terminating_node_ids() const;
  virtual Node *clone(NodeManager &manager, bool recursive = false) const = 0;
  virtual void visit(BDDVisitor &visitor) const = 0;
  virtual std::string dump(bool one_liner = false) const = 0;
  virtual std::string dump_recursive(int lvl = 0) const;

  virtual ~Node() = default;
};

} // namespace bdd