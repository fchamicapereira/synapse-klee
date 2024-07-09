#include "call.h"
#include "manager.h"

#include "../../call-paths-to-bdd.h"
#include "../visitor.h"

namespace bdd {

Node *Call::clone(NodeManager &manager, bool recursive) const {
  Call *clone;

  if (recursive && next) {
    Node *next_clone = next->clone(manager, true);
    clone =
        new Call(id, next_clone, nullptr, constraints, call, generated_symbols);
    next_clone->set_prev(clone);
  } else {
    clone = new Call(id, constraints, call, generated_symbols);
  }

  manager.add_node(clone);
  return clone;
}

void Call::visit(BDDVisitor &visitor) const { visitor.visit(this); }

std::string Call::dump(bool one_liner, bool id_name_only) const {
  std::stringstream ss;
  ss << id << ":";
  ss << call.function_name;

  if (id_name_only) {
    return ss.str();
  }

  ss << "(";

  bool first = true;
  for (auto &arg : call.args) {
    if (!first)
      ss << ", ";
    ss << arg.first << ":";
    ss << kutil::expr_to_string(arg.second.expr, one_liner);
    if (!arg.second.in.isNull() || !arg.second.out.isNull()) {
      ss << "[";
    }
    if (!arg.second.in.isNull()) {
      ss << kutil::expr_to_string(arg.second.in, one_liner);
    }
    if (!arg.second.out.isNull()) {
      ss << " -> ";
      ss << kutil::expr_to_string(arg.second.out, one_liner);
    }
    if (!arg.second.in.isNull() || !arg.second.out.isNull()) {
      ss << "]";
    }
    first = false;
  }
  ss << ")";
  return ss.str();
}

symbols_t Call::get_locally_generated_symbols(
    std::vector<std::string> base_filters) const {
  if (base_filters.empty()) {
    return generated_symbols;
  }

  symbols_t result;
  for (const symbol_t &symbol : generated_symbols) {
    auto found_it =
        std::find(base_filters.begin(), base_filters.end(), symbol.base);
    if (found_it != base_filters.end()) {
      result.insert(symbol);
    }
  }
  return result;
}

} // namespace bdd