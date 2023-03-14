#pragma once

#include <unordered_set>
#include <vector>

#include "load-call-paths.h"
#include "symbol.h"

namespace BDD {

class Node;
class BDDVisitor;

typedef std::shared_ptr<Node> BDDNode_ptr;

class Node {
public:
  enum NodeType { BRANCH, CALL, RETURN_INIT, RETURN_PROCESS, RETURN_RAW };

protected:
  uint64_t id;
  NodeType type;

  BDDNode_ptr next;
  BDDNode_ptr prev;

  std::vector<klee::ConstraintManager> constraints;

public:
  Node(uint64_t _id, NodeType _type)
      : id(_id), type(_type), next(nullptr), prev(nullptr) {}

  Node(uint64_t _id, NodeType _type,
       const std::vector<call_path_t *> &_call_paths)
      : id(_id), type(_type), next(nullptr), prev(nullptr) {
    process_call_paths(_call_paths);
  }

  Node(uint64_t _id, NodeType _type, const BDDNode_ptr &_next,
       const BDDNode_ptr &_prev, const std::vector<call_path_t *> &_call_paths)
      : id(_id), type(_type), next(_next), prev(_prev) {
    process_call_paths(_call_paths);
  }

  Node(uint64_t _id, NodeType _type, const BDDNode_ptr &_next,
       const BDDNode_ptr &_prev,
       const std::vector<klee::ConstraintManager> &_constraints)
      : id(_id), type(_type), next(_next), prev(_prev),
        constraints(_constraints) {}

  void replace_next(const BDDNode_ptr &_next) { next = _next; }

  void add_next(const BDDNode_ptr &_next) {
    assert(next == nullptr);
    assert(_next);

    next = _next;
  }

  void replace_prev(const BDDNode_ptr &_prev) { prev = _prev; }

  void add_prev(const BDDNode_ptr &_prev) {
    assert(prev == nullptr);
    assert(_prev);
    prev = _prev;
  }

  const BDDNode_ptr &get_next() const { return next; }
  const BDDNode_ptr &get_next() { return next; }

  const BDDNode_ptr &get_prev() const { return prev; }
  const BDDNode_ptr &get_prev() { return prev; }

  NodeType get_type() const { return type; }
  uint64_t get_id() const { return id; }


  const std::vector<klee::ConstraintManager> &get_constraints() const {
    return constraints;
  }

  void
  set_constraints(const std::vector<klee::ConstraintManager> &_constraints) {
    constraints = _constraints;
  }

  symbols_t get_all_generated_symbols() const;

  void update_id(uint64_t new_id);

  static std::string process_call_path_filename(std::string call_path_filename);
  void process_call_paths(std::vector<call_path_t *> call_paths);

  virtual std::vector<uint64_t> get_terminating_node_ids() const {
    if (!next) {
      return std::vector<uint64_t>{id};
    }

    return next->get_terminating_node_ids();
  }

  virtual BDDNode_ptr clone(bool recursive = false) const = 0;
  virtual void recursive_update_ids(uint64_t &new_id) = 0;
  virtual void visit(BDDVisitor &visitor) const = 0;
  virtual std::string dump(bool one_liner = false) const = 0;
  virtual std::string dump_recursive(int lvl = 0) const;

protected:
  friend class Call;
  friend class Branch;
  friend class ReturnRaw;
  friend class ReturnInit;
  friend class ReturnProcess;

  virtual std::string get_gv_name() const {
    std::stringstream ss;
    ss << id;
    return ss.str();
  }

  friend class SymbolFactory;
};
} // namespace BDD