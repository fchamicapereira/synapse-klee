#pragma once

#include "../internals/byte.h"
#include "data_structure.h"

#include <vector>

namespace BDD {
namespace emulation {

class Vector : public DataStructure {
private:
  std::vector<bytes_t> data;
  uint64_t elem_size;
  uint64_t capacity;

public:
  Vector(addr_t _obj, uint64_t _elem_size, uint64_t _capacity)
      : DataStructure(VECTOR, _obj), data(_capacity, bytes_t(_elem_size)),
        elem_size(_elem_size), capacity(_capacity) {}

  bytes_t get(int index) const {
    assert(index < (int)capacity);
    return data.at(index);
  }

  void put(int index, bytes_t value) {
    assert(index < (int)capacity);
    assert(value.size == elem_size);
    data[index] = value;
  }

  static Vector *cast(const DataStructureRef &ds) {
    assert(ds->get_type() == DataStructureType::VECTOR);
    return static_cast<Vector *>(ds.get());
  }
};

} // namespace emulation
} // namespace BDD