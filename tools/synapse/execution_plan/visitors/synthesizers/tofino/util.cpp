#include "util.h"
#include "klee-util.h"
#include "pipeline/pipeline.h"
#include "tofino_generator.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

std::string p4_type_from_expr(klee::ref<klee::Expr> expr) {
  auto size_bits = expr->getWidth();
  std::stringstream label;
  label << "bit<" << size_bits << ">";
  return label.str();
}

std::string p4_type_from_expr(bits_t size) {
  std::stringstream label;
  label << "bit<" << size << ">";
  return label.str();
}

std::vector<key_var_t> get_key_vars(Ingress &ingress, klee::ref<klee::Expr> key,
                                    const std::vector<meta_t> &meta) {
  std::vector<key_var_t> key_vars;
  auto free_byte_counter = 0u;

  auto size_bits = key->getWidth();
  auto offset_bits = 0u;

  while (offset_bits < size_bits) {
    auto found = false;

    for (const auto &meta : meta) {
      if (meta.offset != offset_bits) {
        continue;
      }

      auto meta_varq = ingress.search_variable(meta.symbol);

      if (!meta_varq.valid) {
        continue;
      }

      // Careful here, don't go find the size of this by
      // meta_varq.var->get_size_bits(). That method gives the P4 size. That may
      // differ from the expression size, for example in the case of ports (16
      // bits vs 9 bits).
      auto meta_size = meta.size;

      if (size_bits - offset_bits < meta_size) {
        continue;
      }

      auto key_var = key_var_t{*meta_varq.var, offset_bits, false};
      key_vars.push_back(key_var);
      offset_bits += meta_size;
      found = true;
      break;
    }

    if (found) {
      continue;
    }

    auto key_byte =
        kutil::solver_toolbox.exprBuilder->Extract(key, offset_bits, 8);

    auto hdr_varq = ingress.parser.headers.get_field(key_byte);

    if (hdr_varq.valid && hdr_varq.offset_bits == 0) {
      auto hdr_size = hdr_varq.var->get_size_bits();

      if (size_bits - offset_bits >= hdr_size) {
        auto hdr_expr = hdr_varq.var->get_expr();
        auto hdr_chunk = kutil::solver_toolbox.exprBuilder->Extract(
            key, offset_bits, hdr_size);
        auto eq =
            kutil::solver_toolbox.are_exprs_always_equal(hdr_expr, hdr_chunk);

        if (eq) {
          auto key_var = key_var_t{*hdr_varq.var, offset_bits, false};
          key_vars.push_back(key_var);
          offset_bits += hdr_size;
          continue;
        }
      }
    }

    auto key_byte_var = ingress.allocate_key_byte(free_byte_counter);
    auto key_var = key_var_t{key_byte_var, offset_bits, true};
    key_vars.push_back(key_var);
    offset_bits += 8;
    free_byte_counter++;
  }

  return key_vars;
}

} // namespace tofino
} // namespace synthesizer
} // namespace synapse