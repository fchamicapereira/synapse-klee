#include "transpiler.h"
#include "../../../../log.h"
#include "klee-util.h"
#include "tofino_generator.h"
#include "util.h"

#include <assert.h>

using BDD::symbex::CHUNK;
using BDD::symbex::PACKET_LENGTH;

namespace synapse {
namespace synthesizer {
namespace tofino {

class InternalTranspiler : public klee::ExprVisitor::ExprVisitor {
private:
  TofinoGenerator &tg;
  std::stringstream code;

public:
  InternalTranspiler(TofinoGenerator &_tg) : tg(_tg) {}

  std::string get() const { return code.str(); }

private:
  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &);
  klee::ExprVisitor::Action visitSelect(const klee::SelectExpr &);
  klee::ExprVisitor::Action visitConcat(const klee::ConcatExpr &);
  klee::ExprVisitor::Action visitExtract(const klee::ExtractExpr &);
  klee::ExprVisitor::Action visitZExt(const klee::ZExtExpr &);
  klee::ExprVisitor::Action visitSExt(const klee::SExtExpr &);
  klee::ExprVisitor::Action visitAdd(const klee::AddExpr &);
  klee::ExprVisitor::Action visitSub(const klee::SubExpr &);
  klee::ExprVisitor::Action visitMul(const klee::MulExpr &);
  klee::ExprVisitor::Action visitUDiv(const klee::UDivExpr &);
  klee::ExprVisitor::Action visitSDiv(const klee::SDivExpr &);
  klee::ExprVisitor::Action visitURem(const klee::URemExpr &);
  klee::ExprVisitor::Action visitSRem(const klee::SRemExpr &);
  klee::ExprVisitor::Action visitNot(const klee::NotExpr &);
  klee::ExprVisitor::Action visitAnd(const klee::AndExpr &);
  klee::ExprVisitor::Action visitOr(const klee::OrExpr &);
  klee::ExprVisitor::Action visitXor(const klee::XorExpr &);
  klee::ExprVisitor::Action visitShl(const klee::ShlExpr &);
  klee::ExprVisitor::Action visitLShr(const klee::LShrExpr &);
  klee::ExprVisitor::Action visitAShr(const klee::AShrExpr &);
  klee::ExprVisitor::Action visitEq(const klee::EqExpr &);
  klee::ExprVisitor::Action visitNe(const klee::NeExpr &);
  klee::ExprVisitor::Action visitUlt(const klee::UltExpr &);
  klee::ExprVisitor::Action visitUle(const klee::UleExpr &);
  klee::ExprVisitor::Action visitUgt(const klee::UgtExpr &);
  klee::ExprVisitor::Action visitUge(const klee::UgeExpr &);
  klee::ExprVisitor::Action visitSlt(const klee::SltExpr &);
  klee::ExprVisitor::Action visitSle(const klee::SleExpr &);
  klee::ExprVisitor::Action visitSgt(const klee::SgtExpr &);
  klee::ExprVisitor::Action visitSge(const klee::SgeExpr &);
};

std::vector<std::string>
Transpiler::assign_key_bytes(klee::ref<klee::Expr> key,
                             const std::vector<key_var_t> &key_vars) {
  std::vector<std::string> assignments;

  for (const auto &kv : key_vars) {
    if (!kv.is_free) {
      continue;
    }

    auto key_byte =
        kutil::solver_toolbox.exprBuilder->Extract(key, kv.offset_bits, 8);

    auto key_byte_code = tg.transpile(key_byte);

    std::stringstream assignment;

    assignment << kv.variable.get_label();
    assignment << " = ";
    assignment << key_byte_code;

    assignments.push_back(assignment.str());
  }

  return assignments;
}

std::string get_p4_type(klee::Expr::Width width) {
  std::stringstream label;
  label << "bit<" << width << ">";
  return label.str();
}

std::string get_p4_type(klee::ref<klee::Expr> expr) {
  auto sz = expr->getWidth();
  return get_p4_type(sz);
}

std::string mask(std::string expr, bits_t offset, bits_t size) {
  std::stringstream mask_builder;

  assert(size > 1);

  auto lo = offset;
  auto hi = offset + size - 1;

  mask_builder << expr;
  mask_builder << "[";
  mask_builder << hi;
  mask_builder << ":";
  mask_builder << lo;
  mask_builder << "]";

  return mask_builder.str();
}

std::pair<bool, std::string>
try_transpile_constant(TofinoGenerator &tg, const klee::ref<klee::Expr> &expr) {
  auto result = std::pair<bool, std::string>();
  auto is_constant = kutil::is_constant(expr);

  if (is_constant) {
    auto value = kutil::solver_toolbox.value_from_expr(expr);
    auto width = expr->getWidth();

    auto ss = std::stringstream();

    if (width == 1) {
      ss << (value == 0 ? "false" : "true");
    } else {
      assert(width <= 64);
      ss << width << "w" << value;
    }

    result.first = true;
    result.second = ss.str();
  }

  return result;
}

std::pair<bool, std::string>
try_transpile_variable(TofinoGenerator &tg, const klee::ref<klee::Expr> &expr) {
  auto result = std::pair<bool, std::string>();
  auto variable = tg.search_variable(expr);

  if (!variable.valid) {
    return result;
  }

  std::stringstream transpilation_builder;
  auto size_bits = expr->getWidth();

  if (variable.offset_bits > 0 || size_bits < variable.var->get_size_bits()) {
    auto masked =
        mask(variable.var->get_label(), variable.offset_bits, size_bits);
    transpilation_builder << masked;
  } else {
    transpilation_builder << variable.var->get_label();
  }

  result.first = true;
  result.second = transpilation_builder.str();

  return result;
}

struct transpile_var_comparison_t {
  bool valid;
  std::string lhs;
  std::string rhs;

  transpile_var_comparison_t() : valid(false) {}

  transpile_var_comparison_t(const std::string &_lhs, const std::string &_rhs)
      : valid(true), lhs(_lhs), rhs(_rhs) {}
};

transpile_var_comparison_t
transpile_var_comparison(TofinoGenerator &tg, const klee::ref<klee::Expr> &lhs,
                         const klee::ref<klee::Expr> &rhs) {
  auto lhs_varq = tg.search_variable(lhs);
  auto rhs_varq = tg.search_variable(rhs);

  // Not interested if both are variables or both are not variables.
  if (!(lhs_varq.valid ^ rhs_varq.valid)) {
    return transpile_var_comparison_t();
  }

  const auto &varq = lhs_varq.valid ? lhs_varq : rhs_varq;
  const auto &not_var_expr = rhs_varq.valid ? lhs : rhs;

  assert(varq.var);

  if (varq.offset_bits != 0) {
    return transpile_var_comparison_t();
  }

  auto size_bits = varq.var->get_size_bits();

  if (size_bits == 1) {
    auto zero = kutil::solver_toolbox.exprBuilder->Constant(
        0, not_var_expr->getWidth());
    auto eq_zero =
        kutil::solver_toolbox.are_exprs_always_equal(zero, not_var_expr);

    if (eq_zero) {
      return transpile_var_comparison_t(varq.var->get_label(), "false");
    }

    return transpile_var_comparison_t(varq.var->get_label(), "true");
  }

  auto is_constant = kutil::is_constant(not_var_expr);
  assert(is_constant && "TODO");

  auto const_value = kutil::solver_toolbox.value_from_expr(not_var_expr);
  auto new_const_value =
      kutil::solver_toolbox.exprBuilder->Constant(const_value, size_bits);
  auto not_var_transpiled = tg.transpile(new_const_value);

  return transpile_var_comparison_t(varq.var->get_label(), not_var_transpiled);
}

bool detect_packet_length_conditions(klee::ref<klee::Expr> expr) {
  kutil::RetrieveSymbols retriever;
  retriever.visit(expr);
  auto symbols = retriever.get_retrieved_strings();

  std::vector<std::string> allowed_symbols = {
      BDD::symbex::PACKET_LENGTH,
      BDD::symbex::CHUNK,
  };

  // check if there are unexpected symbols

  for (auto symbol : symbols) {
    auto found_it =
        std::find(allowed_symbols.begin(), allowed_symbols.end(), symbol);
    if (found_it == allowed_symbols.end()) {
      return false;
    }
  }

  return symbols.find(BDD::symbex::PACKET_LENGTH) != symbols.end();
}

std::string Transpiler::transpile(const klee::ref<klee::Expr> &expr) {
  auto const_result = try_transpile_constant(tg, expr);

  if (const_result.first) {
    return const_result.second;
  }

  auto simplified = kutil::simplify(expr);
  auto variable_result = try_transpile_variable(tg, simplified);

  if (variable_result.first) {
    return variable_result.second;
  }

  auto transpiler = InternalTranspiler(tg);
  transpiler.visit(simplified);

  auto code = transpiler.get();
  return code;
}

klee::ExprVisitor::Action
InternalTranspiler::visitRead(const klee::ReadExpr &e) {
  klee::ref<klee::Expr> eref = const_cast<klee::ReadExpr *>(&e);
  auto expr_width = eref->getWidth();

  auto symbol = kutil::get_symbol(eref);
  auto variable = tg.search_variable(symbol.second);

  if (!variable.valid) {
    Log::err() << "Unknown variable with symbol " << symbol.second << "\n";
    exit(1);
  }

  auto var_width = variable.var->get_size_bits();
  auto var_label = variable.var->get_label();

  assert(kutil::is_constant(e.index));

  auto index = kutil::solver_toolbox.value_from_expr(e.index);

  if (index == 0 && var_width == expr_width) {
    code << var_label;
    return klee::ExprVisitor::Action::skipChildren();
  }

  assert(expr_width < var_width);

  auto masked = mask(var_label, index * 8, 8);
  code << masked;

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
InternalTranspiler::visitSelect(const klee::SelectExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action
InternalTranspiler::visitConcat(const klee::ConcatExpr &e) {
  auto eref = const_cast<klee::ConcatExpr *>(&e);

  if (kutil::is_readLSB(eref)) {
    auto symbol = kutil::get_symbol(eref);
    auto variable = tg.search_variable(symbol.second);

    if (!variable.valid) {
      Log::err() << "Unknown variable with symbol " << symbol.second << "\n";
      exit(1);
    }

    assert(variable.offset_bits == 0);

    auto expr_width = eref->getWidth();
    auto var_width = variable.var->get_size_bits();

    if (expr_width != var_width) {
      auto max_width = std::max(expr_width, var_width);
      code << "(";
      code << "(";
      code << get_p4_type(max_width);
      code << ")";
      code << variable.var->get_label();
      code << ")";
    } else {
      code << variable.var->get_label();
    }

    return klee::ExprVisitor::Action::skipChildren();
  }

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  std::cerr << kutil::expr_to_string(eref) << "\n";
  assert(false && "TODO");
}

klee::ExprVisitor::Action
InternalTranspiler::visitExtract(const klee::ExtractExpr &e) {
  auto expr = const_cast<klee::ExtractExpr *>(&e);
  auto new_expr = kutil::simplify(expr);

  if (new_expr->getKind() != klee::Expr::Extract) {
    code << tg.transpile(new_expr);
    return klee::ExprVisitor::Action::skipChildren();
  }

  auto new_extract = static_cast<const klee::ExtractExpr *>(new_expr.get());
  auto transpiled = tg.transpile(new_extract->expr);

  code << mask(transpiled, new_extract->offset, new_extract->width);

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
InternalTranspiler::visitZExt(const klee::ZExtExpr &e) {
  auto sz = e.getWidth();
  auto expr = e.getKid(0);
  assert(sz % 8 == 0);

  auto eref = const_cast<klee::ZExtExpr *>(&e);

  if (kutil::is_bool(eref)) {
    code << tg.transpile(expr);
  } else {
    code << "(";
    code << "(" << get_p4_type(eref) << ") ";
    code << "(" << tg.transpile(expr) << ")";
    code << ")";
  }

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
InternalTranspiler::visitSExt(const klee::SExtExpr &e) {
  auto sz = e.getWidth();
  auto expr = e.getKid(0);
  assert(sz % 8 == 0);

  auto eref = const_cast<klee::SExtExpr *>(&e);

  if (kutil::is_bool(eref)) {
    code << tg.transpile(expr);
  } else {
    code << "(";
    code << "(" << get_p4_type(eref) << ") ";
    code << "(" << tg.transpile(expr) << ")";
    code << ")";
  }

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitAdd(const klee::AddExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " + ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitSub(const klee::SubExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " - ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitMul(const klee::MulExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " * ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action
InternalTranspiler::visitUDiv(const klee::UDivExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action
InternalTranspiler::visitSDiv(const klee::SDivExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action
InternalTranspiler::visitURem(const klee::URemExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action
InternalTranspiler::visitSRem(const klee::SRemExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action InternalTranspiler::visitNot(const klee::NotExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action InternalTranspiler::visitAnd(const klee::AndExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  if (detect_packet_length_conditions(lhs)) {
    code << tg.transpile(rhs);
    return klee::ExprVisitor::Action::skipChildren();
  }

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  auto isBool = kutil::is_bool(lhs) || kutil::is_bool(rhs);

  code << "(" << lhs_parsed << ")";
  code << (isBool ? " && " : " & ");
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitOr(const klee::OrExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  /*
(ZExt w32 (Ult (ZExt w64 (Extract w16 0 (Add w32 (w32 4294967282)
                                                              (ZExt w32 (ReadLSB
w16 (w32 0) pkt_len))))) (w64 20)))))
*/

  if (detect_packet_length_conditions(lhs)) {
    code << tg.transpile(rhs);
    return klee::ExprVisitor::Action::skipChildren();
  }

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  auto isBool = kutil::is_bool(lhs) || kutil::is_bool(rhs);

  code << "(" << lhs_parsed << ")";
  code << (isBool ? " || " : " | ");
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitXor(const klee::XorExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action InternalTranspiler::visitShl(const klee::ShlExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action
InternalTranspiler::visitLShr(const klee::LShrExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action
InternalTranspiler::visitAShr(const klee::AShrExpr &e) {
  assert(false && "TODO");
}

klee::ExprVisitor::Action InternalTranspiler::visitEq(const klee::EqExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto transpile_var_comparison_result = transpile_var_comparison(tg, lhs, rhs);

  if (transpile_var_comparison_result.valid) {
    code << transpile_var_comparison_result.lhs;
    code << " == ";
    code << transpile_var_comparison_result.rhs;

    return klee::ExprVisitor::Action::skipChildren();
  }

  auto isBool = kutil::is_bool(lhs) || kutil::is_bool(rhs);

  if (isBool && lhs->getKind() == klee::Expr::Constant) {
    auto constant = static_cast<klee::ConstantExpr *>(lhs.get());
    assert(constant->getWidth() <= 64);
    auto value = constant->getZExtValue();

    code << (value == 0 ? "false" : "true");
  } else {
    auto lhs_code = tg.transpile(lhs);
    code << "(" << lhs_code << ")";
  }

  code << " == ";

  auto rhs_code = tg.transpile(rhs);
  code << "(" << rhs_code << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitNe(const klee::NeExpr &e) {
  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto transpile_var_comparison_result = transpile_var_comparison(tg, lhs, rhs);

  if (transpile_var_comparison_result.valid) {
    code << transpile_var_comparison_result.lhs;
    code << " != ";
    code << transpile_var_comparison_result.rhs;

    return klee::ExprVisitor::Action::skipChildren();
  }

  auto isBool = kutil::is_bool(lhs) || kutil::is_bool(rhs);

  if (isBool && lhs->getKind() == klee::Expr::Constant) {
    auto constant = static_cast<klee::ConstantExpr *>(lhs.get());
    assert(constant->getWidth() <= 64);
    auto value = constant->getZExtValue();

    code << (value == 0 ? "false" : "true");
  } else {
    auto lhs_code = tg.transpile(lhs);
    code << "(" << lhs_code << ")";
  }

  code << " != ";

  auto rhs_code = tg.transpile(rhs);
  code << "(" << rhs_code << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitUlt(const klee::UltExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " < ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitUle(const klee::UleExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " <= ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitUgt(const klee::UgtExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " > ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitUge(const klee::UgeExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " >= ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitSlt(const klee::SltExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " < ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitSle(const klee::SleExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " <= ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitSgt(const klee::SgtExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " > ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

klee::ExprVisitor::Action InternalTranspiler::visitSge(const klee::SgeExpr &e) {
  assert(e.getNumKids() == 2);

  auto lhs = e.getKid(0);
  auto rhs = e.getKid(1);

  auto lhs_parsed = tg.transpile(lhs);
  auto rhs_parsed = tg.transpile(rhs);

  code << "(" << lhs_parsed << ")";
  code << " >= ";
  code << "(" << rhs_parsed << ")";

  return klee::ExprVisitor::Action::skipChildren();
}

Transpiler::parser_cond_data_t
Transpiler::get_parser_cond_data_from_eq(klee::ref<klee::Expr> expr) {
  Transpiler::parser_cond_data_t data;

  assert(!expr.isNull());
  assert(expr->getKind() == klee::Expr::Eq);

  auto lhs = expr->getKid(0);
  auto rhs = expr->getKid(1);

  auto lhs_pkt_dep = kutil::is_packet_readLSB(lhs);

  auto hdr = lhs_pkt_dep ? lhs : rhs;
  auto not_hdr = !lhs_pkt_dep ? lhs : rhs;

  assert(not_hdr->getKind() == klee::Expr::Constant);

  data.hdr = tg.transpile(hdr);
  data.values.push_back(tg.transpile(not_hdr));

  return data;
}

Transpiler::parser_cond_data_t
Transpiler::get_parser_cond_data(klee::ref<klee::Expr> expr) {
  Transpiler::parser_cond_data_t data;

  switch (expr->getKind()) {
  case klee::Expr::Eq: {
    return get_parser_cond_data_from_eq(expr);
  };
  case klee::Expr::Or: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);

    assert(lhs->getKind() == klee::Expr::Eq);
    assert(rhs->getKind() == klee::Expr::Eq);

    auto lhs_data = get_parser_cond_data_from_eq(lhs);
    auto rhs_data = get_parser_cond_data_from_eq(rhs);

    assert(lhs_data.hdr == rhs_data.hdr);

    data.hdr = lhs_data.hdr;

    data.values.insert(data.values.end(), lhs_data.values.begin(),
                       lhs_data.values.end());

    data.values.insert(data.values.end(), rhs_data.values.begin(),
                       rhs_data.values.end());

    return data;
  };
  default:
    assert(false && "TODO");
  }

  return data;
}

} // namespace tofino
} // namespace synthesizer
} // namespace synapse