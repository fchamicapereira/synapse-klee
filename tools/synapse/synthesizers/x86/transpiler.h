#pragma once

#include "klee-util.h"

#include <string>
#include <vector>

#include "domain/variable.h"
#include "../util.h"

namespace synapse {
namespace synthesizer {
namespace x86 {

class x86Generator;

class Transpiler {
private:
  x86Generator &generator;

public:
  Transpiler(x86Generator &_generator) : generator(_generator) {}

  std::string transpile(const klee::ref<klee::Expr> &expr);
  std::string size_to_type(bits_t size, bool is_signed = false) const;
  std::string mask(std::string expr, bits_t offset, bits_t size) const;

  std::pair<bool, std::string>
  try_transpile_variable(const klee::ref<klee::Expr> &expr) const;

  std::pair<bool, std::string>
  try_transpile_constant(const klee::ref<klee::Expr> &expr) const;
};

} // namespace x86
} // namespace synthesizer
} // namespace synapse