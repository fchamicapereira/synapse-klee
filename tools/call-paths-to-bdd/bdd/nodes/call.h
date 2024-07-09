#pragma once

#include <optional>

#include "node.h"

namespace bdd {

class Call : public Node {
private:
  call_t call;
  symbols_t generated_symbols;

public:
  Call(node_id_t _id, const klee::ConstraintManager &_constraints,
       const call_t &_call, const symbols_t &_generated_symbols)
      : Node(_id, NodeType::CALL, _constraints), call(_call),
        generated_symbols(_generated_symbols) {}

  Call(node_id_t _id, Node *_next, Node *_prev,
       const klee::ConstraintManager &_constraints, call_t _call,
       const symbols_t &_generated_symbols)
      : Node(_id, NodeType::CALL, _next, _prev, _constraints), call(_call),
        generated_symbols(_generated_symbols) {}

  const call_t &get_call() const { return call; }
  void set_call(const call_t &new_call) { call = new_call; }

  symbols_t get_locally_generated_symbols(
      std::vector<std::string> base_filters = {}) const;

  void set_locally_generated_symbols(const symbols_t &new_generated_symbols) {
    generated_symbols = new_generated_symbols;
  }

  virtual Node *clone(NodeManager &manager,
                      bool recursive = false) const override;

  void visit(BDDVisitor &visitor) const override;
  std::string dump(bool one_liner = false, bool id_name_only = false) const;
};

} // namespace bdd