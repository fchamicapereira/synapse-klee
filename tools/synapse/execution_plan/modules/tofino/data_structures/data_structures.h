#pragma once

#include <vector>

#include "int_allocator.h"
#include "table.h"
#include "table_non_mergeable.h"

namespace synapse {
namespace targets {
namespace tofino {

class DataStructuresSet {
private:
  std::vector<DataStructureRef> data_structures;

public:
  DataStructuresSet() {}
  DataStructuresSet(const DataStructuresSet &other)
      : data_structures(other.data_structures) {}

  bool contains(DataStructureRef target) const {
    auto it = std::find_if(
        data_structures.begin(), data_structures.end(),
        [&](DataStructureRef ds) { return ds->equals(target.get()); });

    return it != data_structures.end();
  }

  void insert(DataStructureRef ds) {
    if (contains(ds)) {
      return;
    }

    data_structures.push_back(ds);
  }

  const std::vector<DataStructureRef> &get() const { return data_structures; }

  std::vector<DataStructureRef> get_ones_that_implement(obj_addr_t obj) const {
    std::vector<DataStructureRef> implementations;

    for (auto ds : data_structures) {
      if (ds->implements({obj})) {
        implementations.push_back(ds);
      }
    }

    return implementations;
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse