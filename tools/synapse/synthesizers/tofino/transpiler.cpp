#include "transpiler.h"
#include "synthesizer.h"

namespace synapse {
namespace tofino {

Transpiler::Transpiler(const TofinoSynthesizer *_synthesizer)
    : synthesizer(_synthesizer) {}

code_t Transpiler::transpile(klee::ref<klee::Expr> expr) {
  Log::dbg() << "Transpile: " << kutil::expr_to_string(expr, false) << "\n";

  builders.emplace();
  code_builder_t &builder = builders.top();

  bool is_constant = kutil::is_constant(expr);

  if (is_constant) {
    builder << kutil::get_constant_signed(expr);
  } else {
    visit(expr);

    // HACK: clear the visited map so we force the transpiler to revisit all
    // expressions.
    visited.clear();
  }

  code_t code = builder.dump();
  builders.pop();

  assert(code.size() > 0);
  return code;
}

klee::ExprVisitor::Action Transpiler::visitRead(const klee::ReadExpr &e) {
  klee::ref<klee::Expr> expr = const_cast<klee::ReadExpr *>(&e);

  code_builder_t &builder = builders.top();

  code_t var;
  if (synthesizer->get_var(expr, var)) {
    builder << var;
    return klee::ExprVisitor::Action::skipChildren();
  }

  std::cerr << kutil::expr_to_string(expr) << "\n";
  synthesizer->dbg_vars();

  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
Transpiler::visitNotOptimized(const klee::NotOptimizedExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSelect(const klee::SelectExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitConcat(const klee::ConcatExpr &e) {
  klee::ref<klee::Expr> expr = const_cast<klee::ConcatExpr *>(&e);

  code_builder_t &builder = builders.top();

  code_t var;
  if (synthesizer->get_var(expr, var)) {
    builder << var;
    return klee::ExprVisitor::Action::skipChildren();
  }

  std::cerr << kutil::expr_to_string(expr) << "\n";
  synthesizer->dbg_vars();

  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitExtract(const klee::ExtractExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitZExt(const klee::ZExtExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSExt(const klee::SExtExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitAdd(const klee::AddExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " + ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSub(const klee::SubExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " - ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitMul(const klee::MulExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitUDiv(const klee::UDivExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSDiv(const klee::SDivExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitURem(const klee::URemExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSRem(const klee::SRemExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitNot(const klee::NotExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitAnd(const klee::AndExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " & ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitOr(const klee::OrExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " | ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitXor(const klee::XorExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitShl(const klee::ShlExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitLShr(const klee::LShrExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitAShr(const klee::AShrExpr &e) {
  assert(false && "TODO");
  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitEq(const klee::EqExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " == ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitNe(const klee::NeExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " != ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitUlt(const klee::UltExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " < ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitUle(const klee::UleExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " <= ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitUgt(const klee::UgtExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " > ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitUge(const klee::UgeExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " >= ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSlt(const klee::SltExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " < ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSle(const klee::SleExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " <= ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSgt(const klee::SgtExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " > ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action Transpiler::visitSge(const klee::SgeExpr &e) {
  code_builder_t &builder = builders.top();

  klee::ref<klee::Expr> lhs = e.getKid(0);
  klee::ref<klee::Expr> rhs = e.getKid(1);

  builder << transpile(lhs);
  builder << " >= ";
  builder << transpile(rhs);

  return klee::ExprVisitor::Action::skipChildren();
}

} // namespace tofino
} // namespace synapse