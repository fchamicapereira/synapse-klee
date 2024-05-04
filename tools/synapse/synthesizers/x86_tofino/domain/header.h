#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "klee/Expr.h"
#include "variable.h"

#include "../constants.h"

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

enum hdr_field_id_t {
  CPU_DATA_FIELD,
  CPU_CODE_PATH,
  CPU_IN_PORT,
  CPU_OUT_PORT,
  ETH_DST_ADDR,
  ETH_SRC_ADDR,
  ETH_ETHER_TYPE,
  IPV4_VERSION_IHL,
  IPV4_ECN_DSCP,
  IPV4_TOT_LEN,
  IPV4_ID,
  IPV4_FRAG_OFF,
  IPV4_TTL,
  IPV4_PROTOCOL,
  IPV4_CHECK,
  IPV4_SRC_IP,
  IPV4_DST_IP,
  IPV4_OPTIONS_VALUE,
  TCPUDP_SRC_PORT,
  TCPUDP_DST_PORT,
};

struct hdr_field_t : public Variable {
  hdr_field_id_t hdr_field_id;
  klee::ref<klee::Expr> var_length;

  hdr_field_t(hdr_field_id_t _hdr_field_id, const std::string &_label,
              unsigned _size_bits)
      : Variable(_label, _size_bits), hdr_field_id(_hdr_field_id) {}

  hdr_field_t(hdr_field_id_t _hdr_field_id, const std::string &_label,
              klee::ref<klee::Expr> _var_length)
      : Variable(_label, 0 /* mock value */), hdr_field_id(_hdr_field_id),
        var_length(_var_length) {}

  std::string get_type() const override {
    std::stringstream type;

    if (!var_length.isNull()) {
      assert(false && "TODO");
      return type.str();
    }

    return Variable::get_type();
  }
};

enum hdr_id_t {
  CPU,
  ETHERNET,
  IPV4,
  IPV4_OPTIONS,
  TCPUDP,
};

class Header : public Variable {
private:
  hdr_id_t hdr_id;
  std::vector<hdr_field_t> fields;

public:
  Header(hdr_id_t _hdr_id, const std::string &_label,
         const klee::ref<klee::Expr> &_chunk,
         const std::vector<hdr_field_t> &_fields)
      : Variable(_label, _chunk->getWidth()), hdr_id(_hdr_id), fields(_fields) {
    assert(!_chunk.isNull());
    exprs.push_back(_chunk);

    bool size_check = true;
    unsigned total_size_bits = 0;
    auto offset = 0u;

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

  Header(hdr_id_t _hdr_id, const std::string &_label,
         const std::vector<hdr_field_t> &_fields)
      : Variable(_label, 0), hdr_id(_hdr_id), fields(_fields) {
    for (auto &field : fields) {
      auto field_size_bits = field.get_size_bits();
      size_bits += field_size_bits;
    }
  }

public:
  hdr_id_t get_id() const { return hdr_id; }

  const std::vector<hdr_field_t> &query_fields() const { return fields; }

  Variable* get_field_var(hdr_field_id_t hdr_field_id) {
    for (auto &field : fields) {
      if (field.hdr_field_id != hdr_field_id) {
        continue;
      }

      return &field;
    }

    return nullptr;
  }

  variable_query_t query_field_var(hdr_field_id_t hdr_field_id) const {
    for (const auto &field : fields) {
      if (field.hdr_field_id != hdr_field_id) {
        continue;
      }

      std::stringstream field_label;

      field_label << label;
      field_label << "->";
      field_label << field.get_label();

      auto label = field_label.str();
      auto size_bits = field.get_size_bits();

      if (field.has_expr()) {
        auto expr = field.get_expr();
        auto var = Variable(label, size_bits, expr);
        return variable_query_t(var, 0);
      }

      std::vector<std::string> no_symbols;
      auto var = Variable(label, size_bits, no_symbols);
      return variable_query_t(var, 0);
    }

    return variable_query_t();
  }

  variable_query_t query_field(klee::ref<klee::Expr> expr) const {
    auto symbol = kutil::get_symbol(expr);

    if (!symbol.first || symbol.second != BDD::symbex::CHUNK) {
      return variable_query_t();
    }

    for (const auto &field : fields) {
      if (!field.has_expr()) {
        continue;
      }

      auto field_expr = field.get_expr();
      auto contains_result = kutil::solver_toolbox.contains(field_expr, expr);

      if (contains_result.contains) {
        auto field_var = query_field_var(field.hdr_field_id);

        if (expr->getWidth() == 8 && (field.hdr_field_id == ETH_DST_ADDR ||
                                      field.hdr_field_id == ETH_SRC_ADDR)) {
          assert(field_var.var);

          // HACK
          // This should be handled by the transpiler
          std::stringstream new_label;
          new_label << field_var.var->get_label();
          new_label << "[";
          new_label << contains_result.offset_bits / 8;
          new_label << "]";

          auto new_var = Variable(new_label.str(), 8, expr);
          return variable_query_t(new_var, 0);
        }

        field_var.offset_bits = contains_result.offset_bits;
        return field_var;
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

} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse