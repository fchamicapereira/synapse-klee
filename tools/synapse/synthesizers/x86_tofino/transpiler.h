#pragma once

#include "klee-util.h"

#include <string>
#include <vector>

#include "domain/variable.h"

namespace synapse {
namespace synthesizer {
namespace x86_tofino {
class x86TofinoGenerator;

class Transpiler {
private:
  x86TofinoGenerator &tg;

public:
  Transpiler(x86TofinoGenerator &_tg) : tg(_tg) {}

  std::string transpile(const klee::ref<klee::Expr> &expr);
  std::string transpile_lvalue_byte(const Variable &var, bits_t offset);
  std::string size_to_type(bits_t size, bool is_signed = false) const;
  std::string mask(std::string expr, bits_t offset, bits_t size) const;

private:
  std::pair<bool, std::string>
  try_transpile_variable(const klee::ref<klee::Expr> &expr) const;

  std::pair<bool, std::string>
  try_transpile_constant(const klee::ref<klee::Expr> &expr) const;
};
} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse