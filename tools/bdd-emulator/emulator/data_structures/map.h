#pragma once

#include "../internals/byte.h"
#include "data_structure.h"

#include <unordered_map>

namespace BDD {
namespace emulation {

class Map : public DataStructure {
private:
  std::unordered_map<bytes_t, int, bytes_t::hash> data;
  uint64_t capacity;
  uint32_t key_size;

public:
  Map(addr_t _obj, uint64_t _capacity)
      : DataStructure(MAP, _obj), capacity(_capacity), key_size(0) {}

  bool get(bytes_t key, int &value) {
    assert(key.size == key_size || key_size == 0);
    key_size = key.size;

    if (contains(key)) {
      value = data.at(key);
      return true;
    }

    return false;
  }

  bool contains(bytes_t key) const { return data.find(key) != data.end(); }

  void put(bytes_t key, int value) {
    assert(data.size() <= capacity);
    assert(key.size == key_size || key_size == 0);

    key_size = key.size;
    data[key] = value;
  }

  void erase(bytes_t _key) {
    // So, the map uses a hash of the key to index its data.
    // If we provide bigger keys, the hash function doesn't care, as it looks
    // only at the same X first bytes.
    //
    // Some NFs (**cof cof** Firewall **cof cof**) thought it would be a good
    // idea to use a vector to store map keys AND other data. Therefore, the map
    // erase operation actually ignores the extra data it receives, and looks
    // only at the first X bytes.
    //
    // Thus, we sometimes receive keys bigger than the ones we are expecting,
    // and we need to trim them, otherwise we don't encounter the data (because
    // we always hash the entire key... obviously...)

    assert(_key.size >= key_size);
    bytes_t key(_key, _key.size - key_size);

    assert(contains(key));
    assert(key.size == key_size || key_size == 0);

    data.erase(key);
  }

  static Map *cast(const DataStructureRef &ds) {
    assert(ds->get_type() == DataStructureType::MAP);
    return static_cast<Map *>(ds.get());
  }
};

} // namespace emulation
} // namespace BDD