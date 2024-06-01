#pragma once

#include "parser.h"

namespace synapse {
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

} // namespace tofino
} // namespace synapse