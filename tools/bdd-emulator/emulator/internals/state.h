#pragma once

#include "call-paths-to-bdd.h"

#include <memory>
#include <stdint.h>
#include <unordered_map>

namespace BDD {
namespace emulation {

class DataStructure;
typedef std::shared_ptr<DataStructure> DataStructureRef;

struct state_t {
  std::unordered_map<addr_t, DataStructureRef> data_structures;

  void add(const DataStructureRef &ds);
  DataStructureRef get(addr_t obj) const;
};

} // namespace emulation
} // namespace BDD