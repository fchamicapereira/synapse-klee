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

class Headers {
private:
  std::vector<Header> headers;
  CodeBuilder headers_def_builder;
  CodeBuilder headers_decl_builder;

public:
  Headers(int headers_def_ind, int headers_decl_ind)
      : headers_def_builder(headers_def_ind),
        headers_decl_builder(headers_decl_ind) {}

  void add(const Header &hdr) {
    hdr.synthesize_def(headers_def_builder);
    hdr.synthesize_decl(headers_decl_builder);

    headers.push_back(hdr);
  }

  void synthesize(std::ostream &def, std::ostream &decl) {
    headers_def_builder.dump(def);
    headers_decl_builder.dump(decl);
  }

  unsigned get_num_headers() const { return headers.size(); }

  variable_query_t get_field(klee::ref<klee::Expr> chunk) const {
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

  void synthesize_decl(std::ostream &os) {}

private:
  std::unordered_set<Header, Header::hdr_hash_t> get_unique_headers() const {
    std::unordered_set<Header, Header::hdr_hash_t> unique_headers;

    for (const auto &hdr : headers) {
      unique_headers.insert(hdr);
    }

    return unique_headers;
  }
}; // namespace tofino

} // namespace tofino
} // namespace synthesizer
} // namespace synapse