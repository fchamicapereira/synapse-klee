#pragma once

#include "node.h"
#include "return_raw.h"

namespace BDD {
class ReturnProcess : public Node {
public:
  enum Operation { FWD, DROP, BCAST, ERR };

private:
  int value;
  Operation operation;

  std::pair<unsigned, unsigned> analyze_packet_sends(calls_t calls) const;
  void fill_return_value(calls_t calls);

public:
  ReturnProcess(node_id_t _id, const ReturnRaw *raw)
      : Node(_id, Node::NodeType::RETURN_PROCESS, nullptr, nullptr,
             raw->constraints) {
    assert(raw);
    auto calls_list = raw->get_calls();
    assert(calls_list.size());
    fill_return_value(calls_list[0]);
  }

  ReturnProcess(node_id_t _id, const Node_ptr &_prev,
                const klee::ConstraintManager &_constraints, int _value,
                Operation _operation)
      : Node(_id, Node::NodeType::RETURN_PROCESS, nullptr, _prev, _constraints),
        value(_value), operation(_operation) {}

  int get_return_value() const { return value; }
  void set_return_value(int val) { value = val; }
  Operation get_return_operation() const { return operation; }

  virtual Node_ptr clone(bool recursive = false) const override;
  virtual void recursive_update_ids(node_id_t &new_id) override;

  void visit(BDDVisitor &visitor) const override;
  std::string dump(bool one_liner = false) const;
};

#define BDD_CAST_RETURN_PROCESS(NODE_PTR)                                      \
  (static_cast<BDD::ReturnProcess *>((NODE_PTR).get()))

} // namespace BDD