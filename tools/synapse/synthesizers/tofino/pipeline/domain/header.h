#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "klee/Expr.h"
#include "variable.h"

#include "../../../code_builder.h"
#include "../../constants.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

struct hdr_field_t : public Variable {
  klee::ref<klee::Expr> var_length;

  hdr_field_t(const std::string &_label, unsigned _size_bits)
      : Variable(_label, _size_bits) {}

  hdr_field_t(const std::string &_label, unsigned _size_bits,
              klee::ref<klee::Expr> _var_length)
      : Variable(_label, _size_bits), var_length(_var_length) {}

  std::string get_type() const override {
    std::stringstream type;

    if (!var_length.isNull()) {
      type << "varbit<";
      type << size_bits;
      type << ">";

      return type.str();
    }

    return Variable::get_type();
  }
};

class Header : public Variable {
protected:
  std::vector<hdr_field_t> fields;

public:
  Header(const std::string &_label) : Variable(_label, 0) {}

  Header(const std::string &_label, const klee::ref<klee::Expr> &_chunk,
         const std::vector<hdr_field_t> &_fields)
      : Variable(_label, _chunk->getWidth()), fields(_fields) {
    assert(!_chunk.isNull());
    exprs.push_back(_chunk);

    bool size_check = true;
    unsigned total_size_bits = 0;
    auto offset = 0u;

    if (fields.size() == 1 && !fields[0].var_length.isNull()) {
      fields[0].set_expr(_chunk);
      return;
    }

    for (auto &field : fields) {
      auto field_size_bits = field.get_size_bits();

      total_size_bits += field_size_bits;
      size_check &= field.var_length.isNull();

      auto field_expr = kutil::solver_toolbox.exprBuilder->Extract(
          _chunk, offset, field_size_bits);

      field.set_expr(field_expr);
      offset += field_size_bits;
    }

    assert(!size_check || total_size_bits == _chunk->getWidth());
  }

  void synthesize_def(CodeBuilder &builder) const {
    builder.indent();
    builder.append("header ");
    builder.append(label);
    builder.append("_h");
    builder.append(" {");
    builder.append_new_line();

    builder.inc_indentation();

    builder.indent();
    builder.append(fields[0].get_type());
    builder.append(" ");
    builder.append(fields[0].get_label());
    builder.append(";");
    builder.append_new_line();

    builder.dec_indentation();

    builder.indent();
    builder.append("}");
    builder.append_new_line();

    builder.append_new_line();
  }

  void synthesize_decl(CodeBuilder &builder) const {
    builder.indent();
    builder.append(label);
    builder.append("_h");
    builder.append(" ");
    builder.append(label);
    builder.append(";");
    builder.append_new_line();
  }

public:
  const std::vector<hdr_field_t> &query_fields() const { return fields; }

  variable_query_t query_field(klee::ref<klee::Expr> expr) const {
    auto symbol = kutil::get_symbol(expr);

    if (!symbol.first || symbol.second != BDD::symbex::CHUNK) {
      return variable_query_t();
    }

    for (const auto &field : fields) {
      for (auto field_expr : field.get_exprs()) {
        auto contains_result = kutil::solver_toolbox.contains(field_expr, expr);

        if (contains_result.contains) {
          std::stringstream field_label;

          field_label << INGRESS_PACKET_HEADER_VARIABLE;
          field_label << ".";
          field_label << label;
          field_label << ".";
          field_label << field.get_label();

          auto label = field_label.str();
          auto size_bits = field.get_size_bits();
          auto exprs = field.get_exprs();

          auto field_var = Variable(label, size_bits);
          field_var.add_exprs(exprs);

          auto offset_bits = contains_result.offset_bits;

          return variable_query_t(field_var, offset_bits);
        }
      }
    }

    return variable_query_t();
  }

  variable_query_t query_field(const std::string &symbol) const {
    for (const auto &field : fields) {
      if (field.match(symbol)) {
        std::stringstream field_label;

        field_label << INGRESS_PACKET_HEADER_VARIABLE;
        field_label << ".";
        field_label << label;
        field_label << ".";
        field_label << field.get_label();

        auto label = field_label.str();
        auto size_bits = field.get_size_bits();
        auto exprs = field.get_exprs();

        auto field_var = Variable(label, size_bits);        
        field_var.add_exprs(exprs);

        return variable_query_t(field_var, 0);
      }
    }

    return variable_query_t();
  }

  std::string get_type() const override { return label + "_t"; }

  bool operator==(const Header &hdr) const {
    auto this_type = get_type();
    auto other_type = hdr.get_type();

    return hdr.label.compare(label) == 0 && this_type.compare(other_type) == 0;
  }

  struct hdr_hash_t {
    size_t operator()(const Header &hdr) const {
      auto type = hdr.get_type();
      auto hash = std::hash<std::string>()(hdr.label + type);
      return hash;
    }
  };
};

class CustomHeader : public Header {
public:
  CustomHeader(const std::string &_label, const klee::ref<klee::Expr> &_chunk,
               bits_t size)
      : Header(_label, _chunk, {hdr_field_t("data", size)}) {}
};

class CpuHeader : public Header {
public:
  CpuHeader() : Header(CPU_HEADER_LABEL) {}

  void add_field(const hdr_field_t &field) { fields.push_back(field); }
};

} // namespace tofino
} // namespace synthesizer
} // namespace synapse