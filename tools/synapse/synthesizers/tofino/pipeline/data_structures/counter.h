#pragma once

#include <string>
#include <vector>

#include "../../../code_builder.h"
#include "../../constants.h"
#include "../../util.h"
#include "../domain/variable.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

struct counter_t {
  addr_t vector;
  uint64_t capacity;
  bits_t value_size;
  std::pair<bool, uint64_t> max_value;

  counter_t(addr_t _vector, uint64_t _capacity, bits_t _value_size,
            std::pair<bool, uint64_t> _max_value)
      : vector(_vector), capacity(_capacity), value_size(_value_size),
        max_value(_max_value) {}

  static std::string get_label(addr_t vector) {
    std::stringstream label_builder;
    label_builder << "counter_";
    label_builder << vector;
    return label_builder.str();
  }

  static std::string get_value_label(addr_t vector) {
    std::stringstream label_builder;
    label_builder << get_label(vector);
    label_builder << "_value";
    return label_builder.str();
  }

  std::string get_register_label() const {
    std::stringstream label_builder;
    label_builder << get_label(vector);
    label_builder << "_values";
    return label_builder.str();
  }

  std::string get_action_read_and_inc() const {
    std::stringstream label_builder;
    label_builder << get_label(vector);
    label_builder << "_read_and_inc";
    return label_builder.str();
  }

  std::string get_action_read() const {
    std::stringstream label_builder;
    label_builder << get_label(vector);
    label_builder << "_read";
    return label_builder.str();
  }

  std::string build_reg_decl(bits_t value_size, uint64_t capacity,
                             uint64_t initial_value,
                             const std::string &label) const {
    std::stringstream builder;

    auto value_type = p4_type_from_expr(value_size);

    builder << "Register<";
    builder << value_type;
    builder << ", _>(";
    builder << capacity;
    builder << ", ";
    builder << initial_value;
    builder << ") ";
    builder << label;
    builder << ";";

    return builder.str();
  }

  std::vector<std::string>
  fill_template(std::string str,
                const std::vector<std::pair<std::string, std::string>>
                    &replacements) const {
    std::string copy = str;
    for (const auto &replacement : replacements) {
      auto before = "{{" + replacement.first + "}}";
      auto after = replacement.second;

      std::string::size_type n = 0;
      while ((n = copy.find(before, n)) != std::string::npos) {
        copy.replace(n, before.size(), after);
        n += after.size();
      }
    }

    std::stringstream ss(copy);
    std::vector<std::string> lines;
    std::string line;

    while (std::getline(ss, line, '\n')) {
      lines.push_back(line);
    }

    return lines;
  }

  void synthesize_declaration(CodeBuilder &builder) const {
    builder.append_new_line();

    // =================== Registers ===================

    auto reg_label = get_register_label();
    auto reg_decl = build_reg_decl(value_size, capacity, 0, reg_label);

    builder.indent();
    builder.append(reg_decl);
    builder.append_new_line();

    // =================== Actions ===================

    const std::string action_read_template =
        "RegisterAction<{{TYPE}}, _, {{TYPE}}>({{REG}}) {{LABEL}} = {\n"
        "  void apply(inout {{TYPE}} current, out {{TYPE}} out_value) {\n"
        "    out_value = current;\n"
        "  }\n"
        "};\n";

    builder.append_new_line();

    auto value_type = p4_type_from_expr(value_size);

    auto read_action_label = get_action_read();
    auto action_read_lines =
        fill_template(action_read_template, {
                                                {"TYPE", value_type},
                                                {"REG", reg_label},
                                                {"LABEL", read_action_label},
                                            });

    for (const auto &line : action_read_lines) {
      builder.indent();
      builder.append(line);
      builder.append_new_line();
    }

    const std::string action_read_and_inc_template =
        "RegisterAction<{{TYPE}}, _, {{TYPE}}>({{REG}}) {{LABEL}} = {\n"
        "  void apply(inout {{TYPE}} current, out {{TYPE}} out_value) {\n"
        "    out_value = current;\n"
        "    current = current + 1;\n"
        "  }\n"
        "};\n";

    const std::string action_read_and_inc_max_value_template =
        "RegisterAction<{{TYPE}}, _, {{TYPE}}>({{REG}}) {{LABEL}} = {\n"
        "  void apply(inout {{TYPE}} current, out {{TYPE}} out_value) {\n"
        "    out_value = current;\n"
        "    if (current < {{MAX_VALUE}}) {\n"
        "      current = current + 1;\n"
        "    }\n"
        "  }\n"
        "};\n";

    builder.append_new_line();

    auto read_and_inc_action_label = get_action_read_and_inc();
    auto action_read_inc_lines =
        max_value.first
            ? fill_template(action_read_and_inc_max_value_template,
                            {
                                {"TYPE", value_type},
                                {"REG", reg_label},
                                {"LABEL", read_and_inc_action_label},
                                {"MAX_VALUE", std::to_string(max_value.second)},
                            })
            : fill_template(action_read_and_inc_template,
                            {
                                {"TYPE", value_type},
                                {"REG", reg_label},
                                {"LABEL", read_and_inc_action_label},
                            });

    for (const auto &line : action_read_inc_lines) {
      builder.indent();
      builder.append(line);
      builder.append_new_line();
    }
  }

  void synthesize_read(CodeBuilder &builder, const std::string &index,
                       Variable value) const {
    auto read = get_action_read();

    builder.indent();
    builder.append(value.get_type());
    builder.append(" ");
    builder.append(value.get_label());
    builder.append(" = ");
    builder.append(read);
    builder.append(".execute(");
    builder.append(index);
    builder.append(");");
    builder.append_new_line();
  }

  void synthesize_inc(CodeBuilder &builder, const std::string &index,
                      Variable value) const {
    auto read_and_inc = get_action_read_and_inc();

    builder.indent();
    builder.append(value.get_type());
    builder.append(" ");
    builder.append(value.get_label());
    builder.append(" = ");
    builder.append(read_and_inc);
    builder.append(".execute(");
    builder.append(index);
    builder.append(");");
    builder.append_new_line();
  }

  bool operator==(const counter_t &other) const {
    return vector == other.vector;
  }

  struct CounterHashing {
    size_t operator()(const counter_t &counter) const {
      return std::hash<addr_t>()(counter.vector);
    }
  };
};

typedef std::unordered_set<counter_t, counter_t::CounterHashing> counters_t;

} // namespace tofino
} // namespace synthesizer
} // namespace synapse