#pragma once

#include "data_structure.h"

namespace synapse {
namespace targets {
namespace x86 {

struct dchain_t : ds_t {
  uint64_t index_range;

  dchain_t(addr_t _addr, bdd::node_id_t _node_id, uint64_t _index_range)
      : ds_t(ds_type_t::DCHAIN, _addr, _node_id), index_range(_index_range) {}
};

} // namespace x86
} // namespace targets
} // namespace synapse