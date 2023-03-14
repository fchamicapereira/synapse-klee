#pragma once

#include <klee/Expr.h>

#include <vector>
#include <string>

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
};
} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse