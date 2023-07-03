#pragma once

#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include "klee/util/ExprSMTLIBPrinter.h"
#include "klee/util/ExprVisitor.h"

#include "exprs.h"

#include <unordered_map>
#include <unordered_set>

namespace kutil {

class RetrieveSymbols : public klee::ExprVisitor::ExprVisitor {
private:
  std::unordered_map<std::string, klee::UpdateList> roots_updates;
  std::vector<klee::ref<klee::ReadExpr>> retrieved_reads;
  std::vector<klee::ref<klee::ReadExpr>> retrieved_reads_packet_chunks;
  std::vector<klee::ref<klee::Expr>> retrieved_readLSB;
  std::unordered_set<std::string> retrieved_strings;
  bool collapse_readLSB;
  bool stop_on_first_symbol;

public:
  RetrieveSymbols(bool _collapse_readLSB = false,
                  bool _stop_on_first_symbol = false)
      : ExprVisitor(true), collapse_readLSB(_collapse_readLSB),
        stop_on_first_symbol(_stop_on_first_symbol) {}

  klee::ExprVisitor::Action visitConcat(const klee::ConcatExpr &e) {
    klee::ref<klee::Expr> eref = const_cast<klee::ConcatExpr *>(&e);

    if (stop_on_first_symbol) {
      return klee::ExprVisitor::Action::doChildren();
    }

    if (collapse_readLSB && is_readLSB(eref)) {
      retrieved_readLSB.push_back(eref);
      collapse_readLSB = false;
    }

    return klee::ExprVisitor::Action::doChildren();
  }

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    klee::UpdateList ul = e.updates;
    const klee::Array *root = ul.root;

    retrieved_strings.insert(root->name);
    retrieved_reads.emplace_back((const_cast<klee::ReadExpr *>(&e)));
    roots_updates.insert({root->name, ul});

    if (root->name == "packet_chunks") {
      retrieved_reads_packet_chunks.emplace_back(
          (const_cast<klee::ReadExpr *>(&e)));
    }

    if (stop_on_first_symbol) {
      return klee::ExprVisitor::Action::skipChildren();
    }

    return klee::ExprVisitor::Action::doChildren();
  }

  const std::vector<klee::ref<klee::ReadExpr>>& get_retrieved() {
    return retrieved_reads;
  }

  const std::vector<klee::ref<klee::ReadExpr>>& get_retrieved_packet_chunks() {
    return retrieved_reads_packet_chunks;
  }

  const std::vector<klee::ref<klee::Expr>>& get_retrieved_readLSB() {
    return retrieved_readLSB;
  }

  const std::unordered_set<std::string>& get_retrieved_strings() {
    return retrieved_strings;
  }

  const std::unordered_map<std::string, klee::UpdateList> &
  get_retrieved_roots_updates() {
    return roots_updates;
  }

  static bool contains(klee::ref<klee::Expr> expr, const std::string &symbol) {
    RetrieveSymbols retriever;
    retriever.visit(expr);
    auto symbols = retriever.get_retrieved_strings();
    auto found_it = std::find(symbols.begin(), symbols.end(), symbol);
    return found_it != symbols.end();
  }
};

} // namespace kutil