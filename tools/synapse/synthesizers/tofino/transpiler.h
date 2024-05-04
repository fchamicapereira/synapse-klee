#pragma once

#include "klee/util/ExprVisitor.h"

#include "load-call-paths.h"

#include "util.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

class TofinoGenerator;

class Transpiler {
private:
  TofinoGenerator &tg;

public:
  Transpiler(TofinoGenerator &_tg) : tg(_tg) {}

  std::string transpile(const klee::ref<klee::Expr> &expr);
  std::vector<std::string>
  assign_key_bytes(klee::ref<klee::Expr> key,
                   const std::vector<key_var_t> &key_vars);

  struct parser_cond_data_t {
    std::string hdr;
    std::vector<std::string> values;
  };

  parser_cond_data_t get_parser_cond_data(klee::ref<klee::Expr> expr);

private:
  parser_cond_data_t get_parser_cond_data_from_eq(klee::ref<klee::Expr> expr);
};

} // namespace tofino
} // namespace synthesizer
} // namespace synapse