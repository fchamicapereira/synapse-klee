#pragma once

#include <stack>

#include "../../code_builder.h"
#include "../../util.h"
#include "../constants.h"

#include "domain/stack.h"
#include "domain/variable.h"

#include "data_structures/counter.h"
#include "data_structures/hash.h"
#include "data_structures/int_allocator.h"
#include "data_structures/table.h"

#include "headers.h"
#include "parser.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

class Ingress {
private:
  Variables intrinsic_metadata;
  Variables user_metadata;
  Variables key_bytes;

  tables_t tables;
  integer_allocators_t int_allocators;
  counters_t counters;
  hashes_t hashes;

public:
  CodeBuilder state_builder;
  CodeBuilder apply_block_builder;
  CodeBuilder user_metadata_builder;
  CodeBuilder cpu_header_fields_builder;

  Parser parser;

  stack_t local_vars;
  CpuHeader cpu_header;
  PendingIfs pending_ifs;

  Ingress(int headers_def_ind, int headers_decl_ind, int parser_ind,
          int user_meta_ind, int state_ind, int apply_block_ind,
          int cpu_header_fields_ind)
      : state_builder(state_ind), apply_block_builder(apply_block_ind),
        user_metadata_builder(user_meta_ind),
        cpu_header_fields_builder(cpu_header_fields_ind),
        parser(headers_def_ind, headers_decl_ind, parser_ind),
        pending_ifs(apply_block_builder) {
    intrinsic_metadata = std::vector<Variable>{
        {
            INGRESS_INTRINSIC_META_RESUBMIT_FLAG,
            INGRESS_INTRINSIC_META_RESUBMIT_FLAG_SIZE_BITS,
        },

        {
            INGRESS_INTRINSIC_META_PACKET_VERSION,
            INGRESS_INTRINSIC_META_PACKET_VERSION_SIZE_BITS,
        },

        {
            INGRESS_INTRINSIC_META_INGRESS_PORT,
            INGRESS_INTRINSIC_META_INGRESS_PORT_SIZE_BITS,
            {BDD::symbex::PORT, BDD::symbex::PORT2},
        },

        {
            INGRESS_INTRINSIC_META_TIMESTAMP,
            INGRESS_INTRINSIC_META_TIMESTAMP_SIZE_BITS,
        },
    };
  }

  variable_query_t search_variable(std::string symbol) const {
    for (const auto &intrinsic_meta : intrinsic_metadata) {
      if (intrinsic_meta.match(symbol)) {
        return variable_query_t(intrinsic_meta, 0);
      }
    }

    for (const auto &user_meta : user_metadata) {
      if (user_meta.match(symbol)) {
        return variable_query_t(user_meta, 0);
      }
    }

    auto local_var = local_vars.get(symbol);

    if (local_var.valid) {
      return local_var;
    }

    return variable_query_t();
  }

  variable_query_t search_variable(klee::ref<klee::Expr> expr) const {
    for (const auto &intrinsic_meta : intrinsic_metadata) {
      if (intrinsic_meta.match(expr)) {
        return variable_query_t(intrinsic_meta, 0);
      }

      auto contains_result = intrinsic_meta.contains(expr);

      if (contains_result.contains) {
        return variable_query_t(intrinsic_meta, contains_result.offset_bits);
      }
    }

    for (const auto &user_meta : user_metadata) {
      if (user_meta.match(expr)) {
        return variable_query_t(user_meta, 0);
      }

      auto contains_result = user_meta.contains(expr);

      if (contains_result.contains) {
        return variable_query_t(user_meta, contains_result.offset_bits);
      }
    }

    auto local_var = local_vars.get(expr);

    if (local_var.valid) {
      return local_var;
    }

    return variable_query_t();
  }

  Variable allocate_key_byte(int byte) {
    int size = key_bytes.size();

    while (size <= byte) {
      std::stringstream key_byte_label_builder;

      key_byte_label_builder << KEY_BYTE_LABEL;
      key_byte_label_builder << "_";
      key_byte_label_builder << key_bytes.size();

      auto label = key_byte_label_builder.str();
      auto size_bits = 8;

      auto key_byte = Variable(label, size_bits);
      key_byte.synthesize(state_builder);

      key_bytes.push_back(key_byte);
      size = key_bytes.size();
    }

    return key_bytes[byte];
  }

  Variable
  allocate_meta_param(const std::string &table_label, int param_num,
                      const std::vector<klee::ref<klee::Expr>> &meta_params) {
    std::stringstream meta_param_label_builder;

    meta_param_label_builder << table_label;
    meta_param_label_builder << "_";
    meta_param_label_builder << param_num;

    assert(meta_params.size() > 0);

    auto label = meta_param_label_builder.str();
    auto size_bits = meta_params[0]->getWidth();

    auto meta_param_var = Variable(label, size_bits);

    auto found_it =
        std::find(user_metadata.begin(), user_metadata.end(), meta_param_var);

    if (found_it != user_metadata.end()) {
      for (auto param : meta_params) {
        found_it->add_expr(param);
      }
      return *found_it;
    }

    meta_param_var.set_prefix(INGRESS_USER_METADATA_VARIABLE);

    for (auto param : meta_params) {
      meta_param_var.add_expr(param);
    }
    user_metadata.push_back(meta_param_var);

    return meta_param_var;
  }

  Variable allocate_meta(const std::string &label,
                         const std::vector<klee::ref<klee::Expr>> &meta_exprs) {
    assert(meta_exprs.size() > 0);

    auto size_bits = meta_exprs[0]->getWidth();
    auto meta_var = Variable(label, size_bits);

    auto found_it =
        std::find(user_metadata.begin(), user_metadata.end(), meta_var);

    if (found_it != user_metadata.end()) {
      for (auto expr : meta_exprs) {
        found_it->add_expr(expr);
      }
      return *found_it;
    }

    meta_var.set_prefix(INGRESS_USER_METADATA_VARIABLE);

    for (auto expr : meta_exprs) {
      meta_var.add_expr(expr);
    }

    user_metadata.push_back(meta_var);
    return meta_var;
  }

  Variable allocate_meta(const std::string &label, bits_t size) {
    auto meta_var = Variable(label, size);

    auto found_it =
        std::find(user_metadata.begin(), user_metadata.end(), meta_var);

    if (found_it != user_metadata.end()) {
      return *found_it;
    }

    meta_var.set_prefix(INGRESS_USER_METADATA_VARIABLE);
    user_metadata.push_back(meta_var);

    return meta_var;
  }

  Variable allocate_local_auxiliary(const std::string &base_label,
                                    bits_t size) {
    std::vector<klee::ref<klee::Expr>> exprs;
    std::vector<std::string> labels;
    return allocate_local_auxiliary(base_label, size, exprs, labels);
  }

  Variable allocate_local_auxiliary(const std::string &base_label, bits_t size,
                                    const BDD::symbols_t &symbols) {
    std::vector<klee::ref<klee::Expr>> exprs;
    std::vector<std::string> labels;

    for (auto symbol : symbols) {
      exprs.push_back(symbol.expr);
      labels.push_back(symbol.label);
    }

    return allocate_local_auxiliary(base_label, size, exprs, labels);
  }

  Variable
  allocate_local_auxiliary(const std::string &base_label, bits_t size,
                           const std::vector<klee::ref<klee::Expr>> exprs,
                           const std::vector<std::string> &symbols) {
    variable_query_t local_var_query;
    std::stringstream label_builder;
    int counter = -1;

    do {
      counter++;
      label_builder.clear();

      label_builder << base_label;
      label_builder << "_";
      label_builder << counter;

      local_var_query = local_vars.get_by_label(label_builder.str());
    } while (local_var_query.valid);

    auto label = label_builder.str();
    auto var = Variable(label, size);

    if (symbols.size()) {
      var.add_symbols(symbols);
    }

    for (auto expr : exprs) {
      var.add_expr(expr);
    }

    local_vars.append(var);

    return var;
  }

  void set_cpu_hdr_fields(const BDD::symbols_t &fields) {
    for (auto field : fields) {
      auto label = field.label;
      auto size = field.expr->getWidth();

      auto hdr_field = hdr_field_t(label, size);
      hdr_field.add_expr(field.expr);
      hdr_field.add_symbols({field.label});

      cpu_header.add_field(hdr_field);

      cpu_header_fields_builder.indent();
      cpu_header_fields_builder.append(hdr_field.get_type());
      cpu_header_fields_builder.append(" ");
      cpu_header_fields_builder.append(hdr_field.get_label());
      cpu_header_fields_builder.append(";");
      cpu_header_fields_builder.append_new_line();
    }
  }

  variable_query_t get_cpu_hdr_field(const BDD::symbol_t &symbol) {
    auto varq = cpu_header.query_field(symbol.label);

    if (varq.valid) {
      return varq;
    }

    return cpu_header.query_field(symbol.expr);
  }

  void add_table(const table_t &table) { tables.insert(table); }

  const table_t &get_table(const std::string &label) const {
    auto it =
        std::find_if(tables.begin(), tables.end(), [&](const table_t &table) {
          return table.get_label() == label;
        });

    assert(it != tables.end());
    return *it;
  }

  void add_integer_allocator(const integer_allocator_t &int_allocator) {
    int_allocators.insert(int_allocator);
  }

  void add_counter(const counter_t &counter) { counters.insert(counter); }

  const integer_allocator_t &get_int_allocator(addr_t obj) const {
    auto it = std::find_if(int_allocators.begin(), int_allocators.end(),
                           [&](const integer_allocator_t &int_allocator) {
                             return int_allocator.dchain == obj;
                           });

    assert(it != int_allocators.end());
    return *it;
  }

  const counter_t &get_counter(addr_t obj) const {
    auto it = std::find_if(
        counters.begin(), counters.end(),
        [&](const counter_t &counter) { return counter.vector == obj; });

    assert(it != counters.end());
    return *it;
  }

  hash_t get_or_build_hash(bits_t size) {
    auto it =
        std::find_if(hashes.begin(), hashes.end(),
                     [&](const hash_t &hash) { return hash.size == size; });

    if (it == hashes.end()) {
      auto new_hash = hash_t(size, hashes.size());
      hashes.insert(new_hash);
      return new_hash;
    }

    return *it;
  }

  void synthesize_state(std::ostream &os) {
    for (const auto &table : tables) {
      table.synthesize_declaration(state_builder);
    }

    for (const auto &int_allocator : int_allocators) {
      int_allocator.synthesize_declaration(state_builder);
    }

    for (const auto &counter : counters) {
      counter.synthesize_declaration(state_builder);
    }

    for (const auto &hash : hashes) {
      hash.synthesize_declaration(state_builder);
    }

    state_builder.dump(os);
  }

  void synthesize_apply_block(std::ostream &os) {
    apply_block_builder.dump(os);
  }

  void synthesize_user_metadata(std::ostream &os) {
    for (const auto &meta : user_metadata) {
      meta.synthesize(user_metadata_builder);
    }

    user_metadata_builder.dump(os);
  }

  void synthesize_headers(std::ostream &def, std::ostream &decl) {
    parser.synthesize_headers(def, decl);
  }

  void synthesize_parser(std::ostream &os) { parser.synthesize(os); }

  void synthesize_cpu_header(std::ostream &os) {
    cpu_header_fields_builder.dump(os);
  }
}; // namespace tofino

} // namespace tofino
} // namespace synthesizer
} // namespace synapse