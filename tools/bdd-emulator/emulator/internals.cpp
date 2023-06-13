#include "internals.h"
#include "data_structures/data_structures.h"

namespace BDD {
namespace emulation {

void state_t::add(const DataStructureRef &ds) {
  auto obj = ds->get_obj();
  assert(data_structures.find(obj) == data_structures.end());
  data_structures[obj] = ds;
}

DataStructureRef state_t::get(addr_t obj) const {
  assert(data_structures.find(obj) != data_structures.end());
  return data_structures.at(obj);
}

std::unordered_map<node_id_t, emulation::hit_rate_t>
meta_t::get_hit_rate() const {
  std::unordered_map<node_id_t, emulation::hit_rate_t> hit_rate;

  for (auto it = hit_counter.begin(); it != hit_counter.end(); it++) {
    if (packet_counter > 0) {
      hit_rate[it->first] = it->second / (float)(packet_counter);
    } else {
      hit_rate[it->first] = 0;
    }
  }

  return hit_rate;
}

} // namespace emulation
} // namespace BDD