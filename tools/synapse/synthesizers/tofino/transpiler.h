#pragma once

#include "../synthesizer.h"

namespace synapse {
namespace tofino {

class TofinoSynthesizer;

class Transpiler : public klee::ExprVisitor::ExprVisitor {
private:
  std::stack<code_builder_t> builders;
  const TofinoSynthesizer *synthesizer;

public:
  Transpiler(const TofinoSynthesizer *synthesizer);

  code_t transpile(klee::ref<klee::Expr> expr);

  klee::ExprVisitor::Action visitNotOptimized(const klee::NotOptimizedExpr &e);
  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e);
  klee::ExprVisitor::Action visitSelect(const klee::SelectExpr &e);
  klee::ExprVisitor::Action visitConcat(const klee::ConcatExpr &e);
  klee::ExprVisitor::Action visitExtract(const klee::ExtractExpr &e);
  klee::ExprVisitor::Action visitZExt(const klee::ZExtExpr &e);
  klee::ExprVisitor::Action visitSExt(const klee::SExtExpr &e);
  klee::ExprVisitor::Action visitAdd(const klee::AddExpr &e);
  klee::ExprVisitor::Action visitSub(const klee::SubExpr &e);
  klee::ExprVisitor::Action visitMul(const klee::MulExpr &e);
  klee::ExprVisitor::Action visitUDiv(const klee::UDivExpr &e);
  klee::ExprVisitor::Action visitSDiv(const klee::SDivExpr &e);
  klee::ExprVisitor::Action visitURem(const klee::URemExpr &e);
  klee::ExprVisitor::Action visitSRem(const klee::SRemExpr &e);
  klee::ExprVisitor::Action visitNot(const klee::NotExpr &e);
  klee::ExprVisitor::Action visitAnd(const klee::AndExpr &e);
  klee::ExprVisitor::Action visitOr(const klee::OrExpr &e);
  klee::ExprVisitor::Action visitXor(const klee::XorExpr &e);
  klee::ExprVisitor::Action visitShl(const klee::ShlExpr &e);
  klee::ExprVisitor::Action visitLShr(const klee::LShrExpr &e);
  klee::ExprVisitor::Action visitAShr(const klee::AShrExpr &e);
  klee::ExprVisitor::Action visitEq(const klee::EqExpr &e);
  klee::ExprVisitor::Action visitNe(const klee::NeExpr &e);
  klee::ExprVisitor::Action visitUlt(const klee::UltExpr &e);
  klee::ExprVisitor::Action visitUle(const klee::UleExpr &e);
  klee::ExprVisitor::Action visitUgt(const klee::UgtExpr &e);
  klee::ExprVisitor::Action visitUge(const klee::UgeExpr &e);
  klee::ExprVisitor::Action visitSlt(const klee::SltExpr &e);
  klee::ExprVisitor::Action visitSle(const klee::SleExpr &e);
  klee::ExprVisitor::Action visitSgt(const klee::SgtExpr &e);
  klee::ExprVisitor::Action visitSge(const klee::SgeExpr &e);
};

} // namespace tofino
} // namespace synapse