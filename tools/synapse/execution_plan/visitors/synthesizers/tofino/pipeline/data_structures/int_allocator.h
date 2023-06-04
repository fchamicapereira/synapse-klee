#pragma once

#include <string>
#include <vector>

#include "../../../code_builder.h"
#include "../../constants.h"
#include "../../util.h"
#include "table.h"

#include "../domain/variable.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

struct integer_allocator_t {
  addr_t dchain;
  uint64_t capacity;
  bits_t value_size;

  Variable head;
  Variable tail;
  Variable out_of_space;
  Variable allocated;

  table_t query;
  table_t rejuvenation;

  integer_allocator_t(addr_t _dchain, uint64_t _capacity,
                      bits_t _value_size, Variable _head, Variable _tail,
                      Variable _out_of_space, Variable _allocated,
                      table_t _query, table_t _rejuvenation)
      : dchain(_dchain), capacity(_capacity), value_size(_value_size),
        head(_head), tail(_tail), out_of_space(_out_of_space),
        allocated(_allocated), query(_query), rejuvenation(_rejuvenation) {}

  static std::string get_label(addr_t dchain) {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    return label_builder.str();
  }

  static std::string get_query_table_label(addr_t dchain) {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_query";
    return label_builder.str();
  }

  static std::string get_rejuvenation_table_label(addr_t dchain) {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_rejuvenation";
    return label_builder.str();
  }

  static std::string get_head_label(addr_t dchain) {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_head";
    return label_builder.str();
  }

  static std::string get_tail_label(addr_t dchain) {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_tail";
    return label_builder.str();
  }

  std::string get_tail_label() const { return tail.get_label(); }

  std::string get_head_register_label() const {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_head";
    return label_builder.str();
  }

  std::string get_tail_register_label() const {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_tail";
    return label_builder.str();
  }

  std::string get_values_register_label() const {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_values";
    return label_builder.str();
  }

  std::string get_action_read_and_update_head() const {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_read_and_inc_head";
    return label_builder.str();
  }

  std::string get_action_read_tail() const {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_read_tail";
    return label_builder.str();
  }

  std::string get_action_read_value() const {
    std::stringstream label_builder;
    label_builder << "int_allocator_";
    label_builder << dchain;
    label_builder << "_read_value";
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

    auto head_reg = get_head_register_label();
    auto tail_reg = get_tail_register_label();
    auto values_reg = get_values_register_label();

    auto init_head_value = 0;
    auto init_tail_value = capacity - 1;
    assert(init_tail_value > 0);

    auto head_decl = build_reg_decl(value_size, 1, init_head_value, head_reg);
    auto tail_decl = build_reg_decl(value_size, 1, init_tail_value, tail_reg);
    auto values_decl = build_reg_decl(value_size, capacity, 0, values_reg);

    builder.indent();
    builder.append(head_decl);
    builder.append_new_line();

    builder.indent();
    builder.append(tail_decl);
    builder.append_new_line();

    builder.indent();
    builder.append(values_decl);
    builder.append_new_line();

    // =================== Actions ===================

    const std::string action_read_tail_template =
        "RegisterAction<{{TYPE}}, _, {{TYPE}}>({{REG}}) {{LABEL}} = {\n"
        "  void apply(inout {{TYPE}} current_tail, out {{TYPE}} tail) {\n"
        "    tail = current_tail;\n"
        "  }\n"
        "};\n";

    builder.append_new_line();

    auto value_type = p4_type_from_expr(value_size);

    auto read_tail_action_label = get_action_read_tail();
    auto action_read_tail_lines = fill_template(
        action_read_tail_template, {
                                       {"TYPE", value_type},
                                       {"REG", tail_reg},
                                       {"LABEL", read_tail_action_label},
                                   });

    for (const auto &line : action_read_tail_lines) {
      builder.indent();
      builder.append(line);
      builder.append_new_line();
    }

    const std::string action_get_and_inc_head_template =
        "RegisterAction<{{TYPE}}, _, {{TYPE}}>({{REG}}) {{LABEL}} = {\n"
        "  void apply(inout {{TYPE}} current_head, out {{TYPE}} head) {\n"
        "    head = current_head;\n"
        "    if (current_head != {{tail}}) {\n"
        "      current_head = current_head + 1;\n"
        "    }\n"
        "  }\n"
        "};\n";

    builder.append_new_line();

    auto read_update_head_action_label = get_action_read_and_update_head();
    auto tail_label = get_tail_label();
    auto action_read_update_lines =
        fill_template(action_get_and_inc_head_template,
                      {
                          {"TYPE", value_type},
                          {"REG", head_reg},
                          {"LABEL", read_update_head_action_label},
                          {"tail", tail_label},
                      });

    for (const auto &line : action_read_update_lines) {
      builder.indent();
      builder.append(line);
      builder.append_new_line();
    }

    const std::string action_get_value_template =
        "RegisterAction<{{TYPE}}, _, {{TYPE}}>({{REG}}) {{LABEL}} = {\n"
        "  void apply(inout {{TYPE}} current_value, out {{TYPE}} value) {\n"
        "    value = current_value;\n"
        "  }\n"
        "};\n";

    builder.append_new_line();

    auto read_value_action_label = get_action_read_value();
    auto action_read_value_lines = fill_template(
        action_get_value_template, {
                                       {"TYPE", value_type},
                                       {"REG", values_reg},
                                       {"LABEL", read_value_action_label},
                                   });

    for (const auto &line : action_read_value_lines) {
      builder.indent();
      builder.append(line);
      builder.append_new_line();
    }
  }

  void synthesize_allocate(CodeBuilder &builder) const {
    auto read_tail = get_action_read_tail();
    auto read_and_update_head = get_action_read_and_update_head();
    auto read_value = get_action_read_value();

    builder.indent();
    builder.append("// integer allocator: allocate");
    builder.append_new_line();

    builder.indent();
    builder.append(tail.get_label());
    builder.append(" = ");
    builder.append(read_tail);
    builder.append(".execute(0);");
    builder.append_new_line();

    builder.indent();
    builder.append(head.get_type());
    builder.append(" ");
    builder.append(head.get_label());
    builder.append(" = ");
    builder.append(read_and_update_head);
    builder.append(".execute(0);");
    builder.append_new_line();

    builder.indent();
    builder.append(out_of_space.get_type());
    builder.append(" ");
    builder.append(out_of_space.get_label());
    builder.append(" = ");
    builder.append(head.get_label());
    builder.append(" == ");
    builder.append(tail.get_label());
    builder.append(";");
    builder.append_new_line();

    builder.indent();
    builder.append(allocated.get_label());
    builder.append(" = (");
    builder.append(allocated.get_type());
    builder.append(")");
    builder.append(read_value);
    builder.append(".execute(");
    builder.append(head.get_label());
    builder.append(");");
    builder.append_new_line();
  }

  void synthesize_rejuvenate(CodeBuilder &builder) const {
    builder.indent();
    builder.append("// integer allocator: rejuvenate");
    builder.append_new_line();

    rejuvenation.synthesize_apply(builder);
  }

  void synthesize_query(CodeBuilder &builder, Variable hit) const {
    builder.indent();
    builder.append("// integer allocator: query");
    builder.append_new_line();

    query.synthesize_apply(builder, hit);
  }

  bool operator==(const integer_allocator_t &other) const {
    return dchain == other.dchain;
  }

  struct IntegerAllocatorHashing {
    size_t operator()(const integer_allocator_t &allocator) const {
      return std::hash<addr_t>()(allocator.dchain);
    }
  };
};

typedef std::unordered_set<integer_allocator_t,
                           integer_allocator_t::IntegerAllocatorHashing>
    integer_allocators_t;

} // namespace tofino
} // namespace synthesizer
} // namespace synapse