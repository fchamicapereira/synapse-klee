#pragma once

#include <stdint.h>

#define UINT_16_SWAP_ENDIANNESS(p) ((((p)&0xff) << 8) | ((p) >> 8 & 0xff))

namespace synapse {

typedef uint64_t time_ns_t;
typedef uint64_t time_ms_t;

} // namespace synapse