#include "printer.h"
#include "retrieve_symbols.h"

#include <algorithm>
#include <regex>

namespace kutil {

class ExprPrettyPrinter : public klee::ExprVisitor::ExprVisitor {
private:
  std::string result;
  bool use_signed;

public:
  ExprPrettyPrinter(bool _use_signed) : ExprVisitor(false) {
    use_signed = _use_signed;
  }

  ExprPrettyPrinter() : ExprPrettyPrinter(false) {}

  static std::string print(klee::ref<klee::Expr> expr, bool use_signed = true) {
    assert(!expr.isNull());

    if (expr->getKind() != klee::Expr::Kind::Constant) {
      ExprPrettyPrinter printer(use_signed);
      printer.visit(expr);
      return printer.get_result();
    }

    klee::ConstantExpr *constant =
        static_cast<klee::ConstantExpr *>(expr.get());
    std::stringstream ss;

    if (use_signed) {
      switch (constant->getWidth()) {
      case klee::Expr::Bool: {
        bool value = constant->getZExtValue(1);
        ss << value;
        break;
      };
      case klee::Expr::Int8: {
        int8_t value = constant->getZExtValue(8);
        ss << (int)value;
        break;
      };
      case klee::Expr::Int16: {
        int16_t value = constant->getZExtValue(16);
        ss << (int)value;
        break;
      };
      case klee::Expr::Int32: {
        int32_t value = constant->getZExtValue(32);
        ss << (int)value;
        break;
      };
      case klee::Expr::Int64: {
        int64_t value = constant->getZExtValue(64);
        ss << (int)value;
        break;
      };
      default: {
        ss << expr_to_string(expr, true);
        break;
      };
      }
    } else {
      if (constant->getWidth() <= 64) {
        uint64_t value = constant->getZExtValue(constant->getWidth());
        ss << value;
      } else {
        ss << expr_to_string(expr, true);
      }
    }

    return ss.str();
  }

  const std::string &get_result() const { return result; }

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    auto ul = e.updates;
    auto root = ul.root;
    auto index = e.index;

    assert(index->getKind() == klee::Expr::Kind::Constant);

    klee::ConstantExpr *index_const =
        static_cast<klee::ConstantExpr *>(index.get());
    auto i = index_const->getZExtValue();

    std::stringstream ss;
    ss << root->name;
    ss << "[" << i << "]";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSelect(const klee::SelectExpr &e) {
    std::stringstream ss;

    auto cond = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto first = ExprPrettyPrinter::print(e.getKid(1), use_signed);
    auto second = ExprPrettyPrinter::print(e.getKid(2), use_signed);

    ss << cond << " ? " << first << " : " << second;
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  std::vector<std::vector<unsigned>>
  group_bytes(const std::vector<unsigned> bytes) {
    std::vector<std::vector<unsigned>> groups;
    std::vector<unsigned> group;

    if (bytes.size() == 0) {
      return groups;
    }

    for (auto byte : bytes) {
      if (group.size() > 0 && byte + 1 != group.back()) {
        groups.push_back(group);
        group.clear();
      }

      group.push_back(byte);
    }

    if (group.size() > 0) {
      groups.push_back(group);
    }

    return groups;
  }

  klee::ExprVisitor::Action visitConcat(const klee::ConcatExpr &e) {
    klee::ref<klee::Expr> eref = const_cast<klee::ConcatExpr *>(&e);

    std::vector<expr_group_t> groups = get_expr_groups(eref);

    if (groups.size() == 1 && groups[0].has_symbol && groups[0].offset == 0 &&
        groups[0].n_bytes == (eref->getWidth() / 8)) {
      result = groups[0].symbol;
      return klee::ExprVisitor::Action::skipChildren();
    }

    std::stringstream ss;

    for (auto i = 0u; i < groups.size(); i++) {
      if (i > 0) {
        ss << "++";
      }

      const expr_group_t &group = groups[i];

      if (!group.has_symbol) {
        ss << pretty_print_expr(group.expr);
      } else {
        ss << group.symbol;

        if (group.n_bytes == 1) {
          ss << "[" << group.offset << "]";
        } else {
          ss << "[";
          ss << group.offset;
          ss << ":";
          ss << (group.offset + group.n_bytes);
          ss << "]";
        }
      }
    }

    result = ss.str();
    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitExtract(const klee::ExtractExpr &e) {
    auto expr = e.getKid(0);
    auto offset_value = e.offset;

    auto arg = ExprPrettyPrinter::print(expr, use_signed);

    if (offset_value == 0) {
      result = arg;
      return klee::ExprVisitor::Action::skipChildren();
    }

    std::stringstream ss;
    ss << "(Extract " << offset_value << " " << arg << " )";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitZExt(const klee::ZExtExpr &e) {
    result = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSExt(const klee::SExtExpr &e) {
    result = ExprPrettyPrinter::print(e.getKid(0), true);
    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitAdd(const klee::AddExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " + " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSub(const klee::SubExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " - " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitMul(const klee::MulExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " * " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitUDiv(const klee::UDivExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " / " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSDiv(const klee::SDivExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), true);
    auto right = ExprPrettyPrinter::print(e.getKid(1), true);

    ss << "(" << left << " + " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitURem(const klee::URemExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " % " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSRem(const klee::SRemExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), true);
    auto right = ExprPrettyPrinter::print(e.getKid(1), true);

    ss << "(" << left << " % " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitNot(const klee::NotExpr &e) {
    std::stringstream ss;

    auto arg = ExprPrettyPrinter::print(e.getKid(0), use_signed);

    ss << "!" << arg;
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitAnd(const klee::AndExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " & " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitOr(const klee::OrExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " | " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitXor(const klee::XorExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " ^ " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitShl(const klee::ShlExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " << " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitLShr(const klee::LShrExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " >> " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitAShr(const klee::AShrExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), true);
    auto right = ExprPrettyPrinter::print(e.getKid(1), true);

    ss << "(" << left << " >> " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitEq(const klee::EqExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    auto p0 = std::regex(R"(\(0 == (.+)\))");
    auto p1 = std::regex(R"(!(.+))");
    std::smatch m;

    if (left == "0" &&
        (std::regex_match(right, m, p0) || std::regex_match(right, m, p1))) {
      ss << m.str(1);
    } else if (left == "0") {
      ss << "!" << right;
    } else {
      ss << "(" << left << " == " << right << ")";
    }

    result = ss.str();
    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitNe(const klee::NeExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " != " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitUlt(const klee::UltExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " < " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitUle(const klee::UleExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " <= " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitUgt(const klee::UgtExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " > " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitUge(const klee::UgeExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), use_signed);
    auto right = ExprPrettyPrinter::print(e.getKid(1), use_signed);

    ss << "(" << left << " >= " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSlt(const klee::SltExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), true);
    auto right = ExprPrettyPrinter::print(e.getKid(1), true);

    ss << "(" << left << " < " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSle(const klee::SleExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), true);
    auto right = ExprPrettyPrinter::print(e.getKid(1), true);

    ss << "(" << left << " <= " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSgt(const klee::SgtExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), true);
    auto right = ExprPrettyPrinter::print(e.getKid(1), true);

    ss << "(" << left << " > " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }

  klee::ExprVisitor::Action visitSge(const klee::SgeExpr &e) {
    std::stringstream ss;

    auto left = ExprPrettyPrinter::print(e.getKid(0), true);
    auto right = ExprPrettyPrinter::print(e.getKid(1), true);

    ss << "(" << left << " >= " << right << ")";
    result = ss.str();

    return klee::ExprVisitor::Action::skipChildren();
  }
};

std::string pretty_print_expr(klee::ref<klee::Expr> expr, bool use_signed) {
  return ExprPrettyPrinter::print(expr, use_signed);
}

std::string expr_to_string(klee::ref<klee::Expr> expr, bool one_liner) {
  std::string expr_str;
  if (expr.isNull())
    return expr_str;
  llvm::raw_string_ostream os(expr_str);
  expr->print(os);
  os.str();

  if (one_liner) {
    // remove new lines
    expr_str.erase(std::remove(expr_str.begin(), expr_str.end(), '\n'),
                   expr_str.end());

    // remove duplicated whitespaces
    auto bothAreSpaces = [](char lhs, char rhs) -> bool {
      return (lhs == rhs) && (lhs == ' ');
    };
    std::string::iterator new_end =
        std::unique(expr_str.begin(), expr_str.end(), bothAreSpaces);
    expr_str.erase(new_end, expr_str.end());
  }

  return expr_str;
}

} // namespace kutil

std::ostream &operator<<(std::ostream &os, const arg_t &arg) {
  if (arg.fn_ptr_name.first) {
    os << arg.fn_ptr_name.second;
    return os;
  }

  os << kutil::expr_to_string(arg.expr, true);

  if (!arg.in.isNull() || !arg.out.isNull()) {
    os << "[";

    if (!arg.in.isNull()) {
      os << kutil::expr_to_string(arg.in, true);
    }

    os << " -> ";

    if (!arg.out.isNull()) {
      os << kutil::expr_to_string(arg.out, true);
    }

    os << "]";
  }

  return os;
}

std::ostream &operator<<(std::ostream &os, const call_t &call) {
  os << call.function_name;
  os << "(";

  bool first = true;
  for (auto arg_pair : call.args) {
    auto label = arg_pair.first;
    auto arg = arg_pair.second;

    if (!first) {
      os << ",";
    }

    os << label << ":";
    os << arg;

    first = false;
  }

  os << ")";

  if (!call.ret.isNull()) {
    os << " => ";
    os << kutil::expr_to_string(call.ret, true);
  }

  return os;
}

std::ostream &operator<<(std::ostream &str, const call_path_t &cp) {
  str << "Callpath:\n";

  for (auto call : cp.calls) {
    str << "  " << call << "\n";
  }

  return str;
}

std::ostream &operator<<(std::ostream &os, const klee::ref<klee::Expr> &expr) {
  os << kutil::expr_to_string(expr, true);
  return os;
}