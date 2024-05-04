#pragma once

#include "../../code_builder.h"
#include "../constants.h"
#include "domain/header.h"

#include <assert.h>
#include <unordered_set>
#include <vector>

namespace synapse {
namespace synthesizer {
namespace tofino {

class Parser {
private:
  enum state_type_t { INIT, PARSE, BRANCH, FINAL };

  struct __state_t {
    CodeBuilder &builder;
    state_type_t type;
    std::string label;

    std::vector<__state_t *> next;
    __state_t *prev;

    unsigned num_expecting;

    __state_t(CodeBuilder &_builder, state_type_t _type,
              const std::string &_label, unsigned _num_expecting)
        : builder(_builder), type(_type), label(_label), prev(nullptr),
          num_expecting(_num_expecting) {}

    virtual void add_next(__state_t *n) {
      assert(n);
      assert(is_expecting());

      next.push_back(n);
      n->prev = this;
    }

    bool is_expecting() const { return next.size() != num_expecting; }

    virtual void synthesize() = 0;

    virtual ~__state_t() {
      for (auto n : next) {
        assert(n);
        delete n;
      }
    }
  };

  struct __init_t : public __state_t {
    __init_t(CodeBuilder &_builder)
        : __state_t(_builder, INIT, PARSER_STATE_INIT_LABEL, 1) {}

    void synthesize() override {
      builder.indent();

      builder.append("state ");
      builder.append(label);
      builder.append("{");
      builder.append_new_line();
      builder.inc_indentation();

      assert(next.size() == 1);

      builder.indent();
      builder.append("transition ");
      builder.append(next[0]->label);
      builder.append(";");
      builder.append_new_line();

      builder.dec_indentation();
      builder.indent();
      builder.append("}");
      builder.append_new_line();

      builder.append_new_line();
      next[0]->synthesize();
    }
  };

  struct __final_state_t : public __state_t {
    bool accept;

    __final_state_t(CodeBuilder &_builder, bool _accept)
        : __state_t(_builder, FINAL, _accept ? PARSER_ACCEPT : PARSER_REJECT,
                    0),
          accept(_accept) {}

    void synthesize() override {}
  };

  struct __parse_t : public __state_t {
    std::string hdr;

    __parse_t(CodeBuilder &_builder, const std::string &_label,
              const std::string &_hdr, bool expecting)
        : __state_t(_builder, PARSE, _label, 1), hdr(_hdr) {
      if (!expecting) {
        auto accept_state = new __final_state_t(builder, true);
        __state_t::add_next(accept_state);
      }
    }

    void synthesize() override {
      builder.indent();

      builder.append("state ");
      builder.append(label);
      builder.append(" {");
      builder.append_new_line();
      builder.inc_indentation();

      builder.indent();
      builder.append(PARSER_PACKET_VARIABLE_LABEL);
      builder.append(".extract(");
      builder.append(INGRESS_PACKET_HEADER_VARIABLE);
      builder.append(".");
      builder.append(hdr);
      builder.append(");");
      builder.append_new_line();

      assert(next.size() == 1);

      builder.indent();
      builder.append("transition ");
      builder.append(next[0]->label);
      builder.append(";");
      builder.append_new_line();

      builder.dec_indentation();
      builder.indent();
      builder.append("}");
      builder.append_new_line();

      builder.append_new_line();
      next[0]->synthesize();
    }
  };

  struct __branch_t : public __state_t {
    std::string hdr;
    std::vector<std::string> values;
    bool expect_on_false;
    bool reject_on_false;

    __branch_t(CodeBuilder &_builder, const std::string &_label,
               const std::string &_hdr, const std::vector<std::string> &_values,
               bool _expect_on_false, bool _reject_on_false)
        : __state_t(_builder, BRANCH, _label, 2), hdr(_hdr), values(_values),
          expect_on_false(_expect_on_false), reject_on_false(_reject_on_false) {
    }

    void add_next(__state_t *n) override {
      __state_t::add_next(n);

      if (!expect_on_false) {
        auto next_state = new __final_state_t(builder, !reject_on_false);
        __state_t::add_next(next_state);
      }
    }

    void synthesize() override {
      builder.indent();

      builder.append("state ");
      builder.append(label);
      builder.append("{");
      builder.append_new_line();
      builder.inc_indentation();

      assert(next.size() == 2);
      assert(next[0]);
      assert(next[1]);

      builder.indent();
      builder.append("transition select(");
      builder.append(hdr);
      builder.append(") {");
      builder.append_new_line();
      builder.inc_indentation();

      for (auto v : values) {
        builder.indent();
        builder.append(v);
        builder.append(": ");
        builder.append(next[0]->label);
        builder.append(";");
        builder.append_new_line();
      }

      builder.indent();
      builder.append("default: ");
      builder.append(next[1]->label);
      builder.append(";");
      builder.append_new_line();

      builder.dec_indentation();
      builder.indent();
      builder.append("}");
      builder.append_new_line();

      builder.dec_indentation();
      builder.indent();
      builder.append("}");
      builder.append_new_line();

      builder.append_new_line();

      for (auto n : next) {
        n->synthesize();
      }
    }
  };

  struct __state_machine_t {
    CodeBuilder &builder;

    __init_t *start;
    __state_t *leaf;
    int state_counter;

    __state_machine_t(CodeBuilder &_builder)
        : builder(_builder), start(new __init_t(builder)), leaf(start),
          state_counter(0) {}

    std::string build_new_label() {
      std::stringstream label_builder;

      label_builder << "parse_";
      label_builder << state_counter;

      state_counter++;

      return label_builder.str();
    }

    void parse(const std::string &hdr, bool expecting) {
      assert(leaf);
      assert(leaf->type != FINAL);

      auto label = build_new_label();
      auto parse_state = new __parse_t(builder, label, hdr, expecting);

      leaf->add_next(parse_state);
      leaf = parse_state;

      pop();
    }

    void branch(const std::string &hdr, const std::vector<std::string> &values,
                bool expect_on_false, bool reject_on_false) {
      assert(leaf);
      assert(leaf->type != FINAL);

      auto label = build_new_label();
      auto branch_state = new __branch_t(builder, label, hdr, values,
                                         expect_on_false, reject_on_false);

      leaf->add_next(branch_state);
      leaf = branch_state;

      pop();
    }

    void pop() {
      while (!leaf->is_expecting() && leaf->prev) {
        leaf = leaf->prev;
      }
    }

    void synthesize() { start->synthesize(); }

    ~__state_machine_t() {
      assert(start);
      delete start;
    }
  };

private:
  CodeBuilder parser_builder;
  __state_machine_t state_machine;

public:
  Headers headers;

  Parser(int headers_def_ind, int headers_decl_ind, int parser_ind)
      : parser_builder(parser_ind), state_machine(parser_builder),
        headers(headers_def_ind, headers_decl_ind) {}

  void parse_header(klee::ref<klee::Expr> chunk, bits_t size, bool expecting) {
    auto label = build_new_header_label();
    auto header = CustomHeader(label, chunk, size);
    headers.add(header);

    state_machine.parse(label, expecting);
  }

  void add_parsing_condition(const std::string &hdr,
                             const std::vector<std::string> &values,
                             bool expect_on_false, bool reject_on_false) {
    state_machine.branch(hdr, values, expect_on_false, reject_on_false);
  }

  void synthesize_headers(std::ostream &def, std::ostream &decl) {
    headers.synthesize(def, decl);
  }

  void synthesize(std::ostream &os) {
    state_machine.synthesize();
    parser_builder.dump(os);
  }

private:
  std::string build_new_header_label() const {
    std::stringstream builder;
    builder << "hdr";
    builder << headers.get_num_headers();
    return builder.str();
  }
}; // namespace tofino

} // namespace tofino
} // namespace synthesizer
} // namespace synapse