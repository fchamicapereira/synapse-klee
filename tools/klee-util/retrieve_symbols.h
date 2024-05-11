#pragma once

#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include "klee/util/ExprSMTLIBPrinter.h"
#include "klee/util/ExprVisitor.h"

#include "exprs.h"

#include <unordered_map>
#include <unordered_set>

namespace kutil {

class SymbolRetriever : public klee::ExprVisitor::ExprVisitor {
private:
  std::unordered_map<std::string, klee::UpdateList> roots_updates;
  std::vector<klee::ref<klee::ReadExpr>> retrieved_reads;
  std::vector<klee::ref<klee::ReadExpr>> retrieved_reads_packet_chunks;
  std::unordered_set<std::string> retrieved_strings;

public:
  SymbolRetriever() : ExprVisitor(true) {}

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    klee::ref<klee::ReadExpr> expr = const_cast<klee::ReadExpr *>(&e);
    const klee::UpdateList &ul = e.updates;
    const klee::Array *root = ul.root;

    retrieved_strings.insert(root->name);
    retrieved_reads.emplace_back(expr);
    roots_updates.insert({root->name, ul});

    if (root->name == "packet_chunks") {
      retrieved_reads_packet_chunks.emplace_back(expr);
    }

    return klee::ExprVisitor::Action::doChildren();
  }

  const std::vector<klee::ref<klee::ReadExpr>> &get_retrieved() {
    return retrieved_reads;
  }

  const std::vector<klee::ref<klee::ReadExpr>> &get_retrieved_packet_chunks() {
    return retrieved_reads_packet_chunks;
  }

  const std::unordered_set<std::string> &get_retrieved_strings() {
    return retrieved_strings;
  }

  const std::unordered_map<std::string, klee::UpdateList> &
  get_retrieved_roots_updates() {
    return roots_updates;
  }

  static bool contains(klee::ref<klee::Expr> expr, const std::string &symbol) {
    SymbolRetriever retriever;
    retriever.visit(expr);
    auto symbols = retriever.get_retrieved_strings();
    auto found_it = std::find(symbols.begin(), symbols.end(), symbol);
    return found_it != symbols.end();
  }
};

} // namespace kutil