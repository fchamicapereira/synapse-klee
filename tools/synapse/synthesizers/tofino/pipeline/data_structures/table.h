#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../../../code_builder.h"
#include "../../constants.h"
#include "../../util.h"
#include "../domain/variable.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

struct table_t {
  std::string label;
  std::vector<std::vector<key_var_t>> keys_vars;
  std::vector<std::string> key_labels;
  Variables meta_params;
  uint64_t size;

  table_t(const std::string &_label,
          const std::vector<std::vector<key_var_t>> &_keys_vars,
          const Variables &_meta_params, uint64_t _size)
      : label(_label), keys_vars(_keys_vars), meta_params(_meta_params),
        size(_size) {
    for (const auto &key_vars : keys_vars) {
      for (const auto &key_var : key_vars) {
        key_labels.push_back(key_var.variable.get_label());
      }
    }
  }

  table_t(const std::string &_label,
          const std::vector<std::string> &_key_labels,
          const Variables &_meta_params, uint64_t _size)
      : label(_label), key_labels(_key_labels), meta_params(_meta_params),
        size(_size) {}

  std::string get_label() const { return label; }

  std::string get_action_label() const {
    assert(meta_params.size() > 0);
    std::stringstream action_label;
    action_label << label << "_populate";
    return action_label.str();
  }

  std::string get_param_label(int i) const {
    std::stringstream param_label;
    param_label << "param_" << i;
    return param_label.str();
  }

  std::string get_meta_label(int i) const {
    assert(i < (int)meta_params.size());
    return meta_params[i].get_label();
  }

  void synthesize_declaration(CodeBuilder &builder) const {
    builder.append_new_line();

    if (meta_params.size()) {
      auto action_label = get_action_label();

      // =================== Action ===================

      builder.indent();

      builder.append("action ");
      builder.append(action_label);
      builder.append("(");

      // ============== Action Arguments ==============

      for (auto i = 0u; i < meta_params.size(); i++) {
        auto param_label = get_param_label(i);
        auto param_type = meta_params[i].get_type();

        if (i != 0) {
          builder.append(", ");
        }

        builder.append(param_type);
        builder.append(" ");
        builder.append(param_label);
      }

      builder.append(") {");
      builder.append_new_line();

      // =============== Action Body ===============

      builder.inc_indentation();

      for (auto i = 0u; i < meta_params.size(); i++) {
        auto meta_label = get_meta_label(i);
        auto param_label = get_param_label(i);

        builder.indent();

        builder.append(meta_label);
        builder.append(" = ");
        builder.append(param_label);
        builder.append(";");
        builder.append_new_line();
      }

      builder.dec_indentation();
      builder.indent();

      builder.append("}");
      builder.append_new_line();
    }

    // ================== Table ==================

    builder.append_new_line();

    builder.indent();

    builder.append("table ");
    builder.append(label);
    builder.append(" {");
    builder.append_new_line();

    builder.inc_indentation();

    // ================ Table Keys ================

    builder.indent();

    builder.append("key = {");
    builder.append_new_line();

    builder.inc_indentation();

    for (auto key_label : key_labels) {
      builder.indent();

      builder.append(key_label);
      builder.append(": exact;");
      builder.append_new_line();
    }

    builder.dec_indentation();

    builder.indent();

    builder.append("}");
    builder.append_new_line();

    // ================ Table Actions ================

    builder.append_new_line();

    builder.indent();

    builder.append("actions = {");
    builder.append_new_line();
    builder.inc_indentation();

    if (meta_params.size()) {
      auto action_label = get_action_label();

      builder.indent();

      builder.append(action_label);
      builder.append(";");
      builder.append_new_line();
    }

    builder.dec_indentation();
    builder.indent();

    builder.append("}");
    builder.append_new_line();

    // =============== Table Properties ===============

    builder.append_new_line();
    builder.indent();

    builder.append("size = ");
    builder.append(size);
    builder.append(";");
    builder.append_new_line();

    builder.indent();

    builder.append("idle_timeout = true;");
    builder.append_new_line();

    // ================= End Table =================

    builder.dec_indentation();

    builder.indent();

    builder.append("}");
    builder.append_new_line();
  }

  void synthesize_apply(CodeBuilder &builder, Variable hit) const {
    assert(hit.is_active());
    builder.indent();

    builder.append(hit.get_type());
    builder.append(" ");
    builder.append(hit.get_label());
    builder.append(" = ");

    builder.append(label);
    builder.append(".apply().hit;");
    builder.append_new_line();
  }

  void synthesize_apply(CodeBuilder &builder) const {
    builder.indent();
    builder.append(label);
    builder.append(".apply();");
    builder.append_new_line();
  }

  bool operator==(const table_t &other) const { return label == other.label; }
  bool operator==(const std::string &other_label) const {
    return label == other_label;
  }

  struct TableHashing {
    size_t operator()(const table_t &table) const {
      return std::hash<std::string>()(table.label);
    }
  };
};

typedef std::unordered_set<table_t, table_t::TableHashing> tables_t;

} // namespace tofino
} // namespace synthesizer
} // namespace synapse