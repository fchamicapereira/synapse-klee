#include "synthesizer.h"

namespace synapse {
namespace tofino {

void TofinoSynthesizer::transpile_table(
    code_builder_t &builder, const Table *table,
    const std::vector<klee::ref<klee::Expr>> &keys,
    const std::vector<klee::ref<klee::Expr>> &values) {
  code_t action_name = table->id + "_get_value";

  std::vector<code_t> action_params;
  size_t total_values = values.size();
  for (size_t i = 0; i < total_values; i++) {
    klee::ref<klee::Expr> value = values[i];
    code_t param = table->id + "_value_" + std::to_string(i);
    action_params.push_back(param);
    var_stacks.back().emplace_back(param, value);

    builder.indent();
    builder << type_from_expr(value);
    builder << " ";
    builder << param;
    builder << ";\n";
  }

  if (!values.empty()) {
    builder.indent();
    builder << action_name << "(";

    for (size_t i = 0; i < total_values; i++) {
      klee::ref<klee::Expr> value = values[i];

      if (i != 0) {
        builder << ", ";
      }

      builder << type_from_expr(value);
      builder << " ";
      builder << "_" << action_params[i];
    }

    builder << ") {\n";

    builder.inc();

    for (const code_t &param : action_params) {
      builder.indent();
      builder << param;
      builder << " = ";
      builder << "_" << param;
      builder << ";\n";
    }

    builder.dec();
    builder.indent();
    builder << "}\n";

    builder.indent();
    builder << "\n";
  }

  builder.indent();
  builder << "table " << table->id << " {\n";
  builder.inc();

  builder.indent();
  builder << "key = {\n";
  builder.inc();

  for (klee::ref<klee::Expr> key : keys) {
    builder.indent();
    builder << transpiler.transpile(key) << ": exact;\n";
  }

  builder.dec();
  builder.indent();
  builder << "}\n";

  builder.indent();
  builder << "actions = {";

  if (!values.empty()) {
    builder << "\n";
    builder.inc();

    builder.indent();
    builder << action_name << ";\n";

    builder.dec();
    builder.indent();
  }

  builder << "}\n";

  builder.indent();
  builder << "size = " << table->num_entries << ";\n";

  builder.dec();
  builder.indent();
  builder << "}\n";
  builder << "\n";
}

} // namespace tofino
} // namespace synapse