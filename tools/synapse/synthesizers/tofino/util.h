#pragma once

#include "pipeline/domain/variable.h"

#include "klee/Expr.h"

#include <string>
#include <vector>

namespace synapse {
namespace synthesizer {
namespace tofino {

struct key_var_t {
  Variable variable;
  unsigned offset_bits;
  bool is_free;
};

class Ingress;

std::string p4_type_from_expr(klee::ref<klee::Expr> expr);
std::string p4_type_from_expr(bits_t size);
std::vector<key_var_t> get_key_vars(Ingress &ingress, klee::ref<klee::Expr> key,
                                    const std::vector<meta_t> &meta);
} // namespace tofino
} // namespace synthesizer
} // namespace synapse