#pragma once

#include <memory>
#include <string>

#include "../../../code_builder.h"
#include "../../constants.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

struct state_t {
  enum type_t { INITIAL, CONDITIONAL, EXTRACTOR, TERMINATOR };

  type_t type;
  std::string label;

  state_t(type_t _type, std::string _label) : type(_type), label(_label) {}

  virtual void synthesize(CodeBuilder &builder) const = 0;
};

struct initial_state_t : state_t {
  std::shared_ptr<state_t> next;

  initial_state_t()
      : state_t(INITIAL, PARSER_INITIAL_STATE_LABEL), next(nullptr) {}

  void synthesize(CodeBuilder &builder) const override {
    assert(next);

    builder.indent();
    builder.append("state ");
    builder.append(label);
    builder.append(" {");
    builder.append_new_line();

    builder.inc_indentation();

    builder.indent();
    builder.append("transition ");
    builder.append(next->label);
    builder.append(";");
    builder.append_new_line();

    builder.dec_indentation();

    builder.indent();
    builder.append("}");
    builder.append_new_line();
  }
};

struct conditional_state_t : state_t {
  std::string condition;
  bool transition_value;

  std::shared_ptr<state_t> next_on_true;
  std::shared_ptr<state_t> next_on_false;

  conditional_state_t(const std::string &_label, const std::string &_condition)
      : state_t(CONDITIONAL, _label), condition(_condition) {}

  void synthesize(CodeBuilder &builder) const override {
    assert(condition.size());

    builder.append_new_line();

    builder.indent();
    builder.append("state ");
    builder.append(label);
    builder.append(" {");
    builder.append_new_line();

    builder.inc_indentation();

    builder.indent();
    builder.append("transition select(");
    builder.append(condition);
    builder.append(") {");
    builder.append_new_line();

    builder.inc_indentation();

    assert(next_on_true || next_on_false);

    if (next_on_true) {
      builder.indent();
      builder.append("true: ");
      builder.append(next_on_true->label);
      builder.append(";");
      builder.append_new_line();
    }

    if (next_on_false) {
      builder.indent();
      builder.append("false: ");
      builder.append(next_on_false->label);
      builder.append(";");
      builder.append_new_line();
    }

    builder.dec_indentation();

    builder.indent();
    builder.append("}");
    builder.append_new_line();

    builder.dec_indentation();

    builder.indent();
    builder.append("}");
    builder.append_new_line();
  }
};

struct extractor_state_t : state_t {
  std::string hdr;
  std::string dynamic_length;
  std::shared_ptr<state_t> next;

  extractor_state_t(const std::string &_label, const std::string &_hdr,
                    const std::string &_dynamic_length)
      : state_t(EXTRACTOR, _label), hdr(_hdr), dynamic_length(_dynamic_length) {

  }

  void synthesize(CodeBuilder &builder) const override {
    assert(next);

    builder.append_new_line();

    builder.indent();
    builder.append("state ");
    builder.append(label);
    builder.append(" {");
    builder.append_new_line();

    builder.inc_indentation();

    builder.indent();
    builder.append(PARSER_PACKET_VARIABLE_LABEL);
    builder.append(".extract(hdr.");
    builder.append(hdr);

    if (dynamic_length.size()) {
      builder.append(", ");
      builder.append(dynamic_length);
    }

    builder.append(");");
    builder.append_new_line();

    builder.indent();
    builder.append("transition ");
    builder.append(next->label);
    builder.append(";");
    builder.append_new_line();

    builder.dec_indentation();

    builder.indent();
    builder.append("}");
    builder.append_new_line();
  }
};

struct terminator_state_t : state_t {
  terminator_state_t(const std::string &_label) : state_t(TERMINATOR, _label) {}

  void synthesize(CodeBuilder &builder) const override {}
};

} // namespace tofino
} // namespace synthesizer
} // namespace synapse