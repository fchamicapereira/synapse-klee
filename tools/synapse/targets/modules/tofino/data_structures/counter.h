#pragma once

#include <unordered_set>
#include <vector>

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "data_structure.h"
#include "table.h"

namespace synapse {
namespace targets {
namespace tofino {

class Counter;
typedef std::shared_ptr<Counter> CounterRef;

class Counter : public DataStructure {
private:
  uint64_t capacity;
  bits_t value_size;
  std::pair<bool, uint64_t> max_value;

public:
  Counter(addr_t _obj, const std::unordered_set<BDD::node_id_t> &_nodes,
          uint64_t _capacity, bits_t _value_size)
      : DataStructure(Type::COUNTER, {_obj}, _nodes), capacity(_capacity),
        value_size(_value_size), max_value({false, 0}) {}

  Counter(addr_t _obj, const std::unordered_set<BDD::node_id_t> &_nodes,
          uint64_t _capacity, bits_t _value_size, uint64_t _max_value)
      : DataStructure(Type::COUNTER, {_obj}, _nodes), capacity(_capacity),
        value_size(_value_size), max_value({true, _max_value}) {}

  bool operator==(const Counter &other) const {
    assert(objs.size() == 1);
    assert(other.objs.size() == 1);
    return *objs.begin() == *other.objs.begin();
  }

  uint64_t get_capacity() const { return capacity; }
  bits_t get_value_size() const { return value_size; }
  const std::pair<bool, uint64_t> &get_max_value() const { return max_value; }

  static CounterRef build(addr_t _obj,
                          const std::unordered_set<BDD::node_id_t> &_nodes,
                          uint64_t _capacity, bits_t _value_size,
                          std::pair<bool, uint64_t> _max_value) {
    if (_max_value.first) {
      return CounterRef(
          new Counter(_obj, _nodes, _capacity, _value_size, _max_value.second));
    }
    return CounterRef(new Counter(_obj, _nodes, _capacity, _value_size));
  }

  bool equals(const Counter *other) const {
    if (!DataStructure::equals(other)) {
      return false;
    }

    assert(objs.size() == 1);
    assert(other->objs.size() == 1);

    if (*objs.begin() != *other->objs.begin()) {
      return false;
    }

    return true;
  }

  virtual bool equals(const DataStructure *other) const override {
    if (!DataStructure::equals(other)) {
      return false;
    }

    auto other_casted = static_cast<const Counter *>(other);
    return equals(other_casted);
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
