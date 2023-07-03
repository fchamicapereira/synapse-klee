#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "base-types.h"
#include "cfg.h"
#include "context.h"
#include "meta.h"
#include "packet.h"
#include "state.h"
#include "utils.h"

#include <assert.h>
#include <unordered_map>

namespace BDD {
namespace emulation {

typedef void (*operation_ptr)(const BDD &bdd, const Call *call, pkt_t &pkt,
                              time_ns_t time, state_t &state, meta_t &meta,
                              context_t &ctx, const cfg_t &cfg);

typedef std::map<std::string, operation_ptr> operations_t;

} // namespace emulation
} // namespace BDD