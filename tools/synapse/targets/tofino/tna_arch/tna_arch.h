#pragma once

#include "parser.h"

#include "klee-util.h"

#include <unordered_map>

namespace synapse {
class EP;
namespace tofino {

enum class TNAVersion { TNA1, TNA2 };

enum class TNAProperty {
  MAX_PACKET_BYTES_IN_CONDITION,
  STAGES,
  SRAM_PER_STAGE_BITS,
  TCAM_PER_STAGE_BITS,
  MAX_LOGICAL_TCAM_TABLES,
  MAX_LOGICAL_SRAM_AND_TCAM_TABLES,
  PHV_SIZE_BITS,
  PHV_8BIT_CONTAINERS,
  PHV_16BIT_CONTAINERS,
  PHV_32BIT_CONTAINERS,
  PACKET_BUFFER_SIZE_BITS,
  EXACT_MATCH_XBAR_BITS,
  MAX_EXACT_MATCH_KEYS,
  TERNARY_MATCH_XBAR_BITS,
  MAX_TERNARY_MATCH_KEYS,
};

class TNA {
private:
  std::unordered_map<TNAProperty, int> properties;
  Parser parser;

public:
  TNA(TNAVersion version);

  TNA(const TNA &other) : properties(other.properties), parser(other.parser) {}

  // Tofino compiler complains if we access more than 4 bytes of the packet on
  // the same if statement.
  bool condition_meets_phv_limit(klee::ref<klee::Expr> expr) const;

  void update_parser_transition(const EP *ep, klee::ref<klee::Expr> hdr);
  void update_parser_condition(const EP *ep, klee::ref<klee::Expr> condition);
  void update_parser_accept(const EP *ep);
  void update_parser_reject(const EP *ep);
};

} // namespace tofino
} // namespace synapse