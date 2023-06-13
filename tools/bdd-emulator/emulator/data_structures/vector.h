#pragma once

#include "byte.h"
#include "data_structure.h"

#include <unordered_map>

namespace BDD {
namespace emulation {

class Vector : public DataStructure {
private:
  std::unordered_map<int, bytes_t> data;
  uint64_t elem_size;
  uint64_t capacity;

public:
  Vector(addr_t _obj, uint64_t _elem_size, uint64_t _capacity)
      : DataStructure(VECTOR, _obj), elem_size(_elem_size),
        capacity(_capacity) {}

  bytes_t get(int index) const {
    assert(index < (int)capacity);

    if (contains(index)) {
      return data.at(index);
    }

    return bytes_t(elem_size);
  }

  bool contains(int index) const { return data.find(index) != data.end(); }

  void put(int index, bytes_t value) {
    assert(index < (int)capacity);
    data[index] = value;
  }

  void clear(int index) {
    assert(index < (int)capacity);
    assert(contains(index));

    auto &value = data[index];

    for (auto byte = 0u; byte < value.size; byte++) {
      value[byte] = 0;
    }
  }

  static Vector *cast(const DataStructureRef &ds) {
    assert(ds->get_type() == DataStructureType::VECTOR);
    return static_cast<Vector *>(ds.get());
  }
};

} // namespace emulation
} // namespace BDD