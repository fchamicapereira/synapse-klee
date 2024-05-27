#pragma once

#include <assert.h>
#include <iostream>

#include "../bdd.h"
#include "../nodes/nodes.h"
#include "../visitor.h"

#include "klee-util.h"

namespace bdd {

class PrinterDebug : public BDDVisitor {
private:
  bool traverse;

public:
  static void debug(const Node *node) {
    PrinterDebug debug(false);
    node->visit(debug);
  }

public:
  PrinterDebug(bool _traverse) : traverse(_traverse) {}
  PrinterDebug() : PrinterDebug(true) {}

  void visit(const BDD &bdd) override {
    const Node *root = bdd.get_root();
    assert(root);

    klee::ref<klee::Expr> device = bdd.get_device().expr;
    klee::ref<klee::Expr> packet_len = bdd.get_packet_len().expr;
    klee::ref<klee::Expr> time = bdd.get_time().expr;

    std::cerr << "===========================================\n";
    std::cerr << "Init calls:\n";
    for (const call_t &call : bdd.get_init()) {
      std::cerr << "\t" << call << "\n";
    }
    std::cerr << "===========================================\n";

    visitRoot(root);

    std::cerr << "===========================================\n";
    std::cerr << "Base symbols:\n";
    std::cerr << "  " << kutil::expr_to_string(device, true) << "\n";
    std::cerr << "  " << kutil::expr_to_string(packet_len, true) << "\n";
    std::cerr << "  " << kutil::expr_to_string(time, true) << "\n";
    std::cerr << "===========================================\n";
  }

  BDDVisitorAction visitBranch(const Branch *node) override {
    node_id_t id = node->get_id();
    klee::ref<klee::Expr> condition = node->get_condition();

    const Node *on_true = node->get_on_true();
    const Node *on_false = node->get_on_false();

    std::string on_true_id = on_true ? std::to_string(on_true->get_id()) : "X";
    std::string on_false_id =
        on_false ? std::to_string(on_false->get_id()) : "X";

    std::cerr << "===========================================\n";
    std::cerr << "node:      " << id << "\n";
    std::cerr << "type:      branch\n";
    std::cerr << "condition: ";
    condition->dump();
    std::cerr << "on true:   " << on_true_id << "\n";
    std::cerr << "on false:  " << on_false_id << "\n";
    visitConstraints(node);
    std::cerr << "===========================================\n";

    return traverse ? BDDVisitorAction::VISIT_CHILDREN : BDDVisitorAction::STOP;
  }

  BDDVisitorAction visitCall(const Call *node) override {
    node_id_t id = node->get_id();
    const call_t &call = node->get_call();
    const Node *next = node->get_next();

    std::cerr << "===========================================\n";
    std::cerr << "node:      " << id << "\n";
    std::cerr << "type:      call\n";
    std::cerr << "function:  " << call.function_name << "\n";
    std::cerr << "args:      ";
    bool indent = false;
    for (const auto &arg : call.args) {
      if (indent)
        std::cerr << "           ";
      std::cerr << arg.first << " : ";
      arg.second.expr->dump();
      indent = true;
    }
    if (call.args.size() == 0) {
      std::cerr << "\n";
    }
    if (!call.ret.isNull()) {
      std::cerr << "ret:       ";
      call.ret->dump();
    }
    if (next) {
      std::cerr << "next:      " << next->get_id() << "\n";
    }
    visitConstraints(node);
    std::cerr << "===========================================\n";

    return traverse ? BDDVisitorAction::VISIT_CHILDREN : BDDVisitorAction::STOP;
  }

  BDDVisitorAction visitRoute(const Route *node) override {
    node_id_t id = node->get_id();
    uint64_t dst_device = node->get_dst_device();
    RouteOperation operation = node->get_operation();
    const Node *next = node->get_next();

    std::cerr << "===========================================\n";
    std::cerr << "node:      " << id << "\n";
    std::cerr << "type:      route\n";
    std::cerr << "operation: ";
    switch (operation) {
    case RouteOperation::FWD: {
      std::cerr << "fwd(" << dst_device << ")";
      break;
    }
    case RouteOperation::DROP: {
      std::cerr << "drop()";
      break;
    }
    case RouteOperation::BCAST: {
      std::cerr << "bcast()";
      break;
    }
    }
    std::cerr << "\n";
    if (next) {
      std::cerr << "next:      " << next->get_id() << "\n";
    }
    visitConstraints(node);
    std::cerr << "===========================================\n";

    return traverse ? BDDVisitorAction::VISIT_CHILDREN : BDDVisitorAction::STOP;
  }

  void visitRoot(const Node *root) override { root->visit(*this); }

private:
  void visitConstraints(const Node *node) {
    const auto &constraints = node->get_constraints();
    if (constraints.size() > 0) {
      std::cerr << "constraints:\n";
      for (const auto &constraint : constraints) {
        std::cerr << "  " << kutil::expr_to_string(constraint, true) << "\n";
      }
    }
  }
};

} // namespace bdd
