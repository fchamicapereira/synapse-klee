#pragma once

#include "../../code_builder.h"
#include "../constants.h"
#include "header.h"

#include <assert.h>
#include <unordered_set>
#include <vector>

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

class Headers {
private:
  std::vector<Header> headers;

public:
  void add(const Header &hdr) { headers.push_back(hdr); }

  variable_query_t get_hdr(hdr_id_t hdr_id) const {
    for (auto hdr : headers) {
      if (hdr.get_id() != hdr_id) {
        continue;
      }

      return variable_query_t(hdr, 0);
    }

    return variable_query_t();
  }

  Variable* get_hdr_field(hdr_id_t hdr_id, hdr_field_id_t field_id) {
    for (auto& hdr : headers) {
      if (hdr.get_id() != hdr_id) {
        continue;
      }

      return hdr.get_field_var(field_id);
    }

    return nullptr;
  }

  variable_query_t query_hdr_field(hdr_id_t hdr_id, hdr_field_id_t field_id) const {
    for (auto hdr : headers) {
      if (hdr.get_id() != hdr_id) {
        continue;
      }

      return hdr.query_field_var(field_id);
    }

    return variable_query_t();
  }

  variable_query_t query_hdr_field_from_chunk(klee::ref<klee::Expr> chunk) const {
    auto symbol = kutil::get_symbol(chunk);

    if (!symbol.first || symbol.second != BDD::symbex::CHUNK) {
      return variable_query_t();
    }

    for (const auto &hdr : headers) {
      auto varq = hdr.query_field(chunk);

      if (varq.valid) {
        return varq;
      }
    }

    return variable_query_t();
  }

}; // namespace tofino

} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse