#include "state.h"
#include "../data_structures/data_structures.h"

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

} // namespace emulation
} // namespace BDD