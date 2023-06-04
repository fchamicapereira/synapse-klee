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

class IntegerAllocator;
typedef std::shared_ptr<IntegerAllocator> IntegerAllocatorRef;

class IntegerAllocator : public DataStructure {
private:
  Table query;
  Table rejuvenation;
  std::vector<klee::ref<klee::Expr>> integer;
  BDD::symbols_t out_of_space;

  uint64_t capacity;
  bits_t integer_size;

public:
  IntegerAllocator(klee::ref<klee::Expr> _integer,
                   const BDD::symbol_t &_out_of_space, uint64_t _capacity,
                   addr_t _obj,
                   const std::unordered_set<BDD::node_id_t> &_nodes)
      : IntegerAllocator(_capacity, _obj, _nodes) {
    integer.push_back(_integer);
    out_of_space.insert(_out_of_space);
  }

  IntegerAllocator(uint64_t _capacity, addr_t _obj,
                   const std::unordered_set<BDD::node_id_t> &_nodes)
      : DataStructure(Type::INTEGER_ALLOCATOR, {_obj}, _nodes),
        query("int_allocator_query", {}, {}, {}, {_obj}, _nodes),
        rejuvenation("int_allocator_rejuvenation", {}, {}, {}, {_obj}, _nodes),
        capacity(_capacity) {
    integer_size = bits_required_for_capacity(capacity);
  }

  void add_integer(klee::ref<klee::Expr> other_integer) {
    assert(!other_integer.isNull());
    integer.push_back(other_integer);
  }

  void add_out_of_space(const BDD::symbol_t &other_out_of_space) {
    out_of_space.insert(other_out_of_space);
  }

  void add_is_allocated(const BDD::symbol_t &is_allocated) {
    query.add_hit({is_allocated});
    rejuvenation.add_hit({is_allocated});
  }

  bool operator==(const IntegerAllocator &other) const {
    assert(objs.size() == 1);
    assert(other.objs.size() == 1);
    return *objs.begin() == *other.objs.begin();
  }

  const Table &get_query_table() const { return query; }
  const Table &get_rejuvenation_table() const { return rejuvenation; }

  const std::vector<klee::ref<klee::Expr>> &get_integer() const {
    return integer;
  }

  const BDD::symbols_t &get_out_of_space() const { return out_of_space; }

  bits_t get_integer_size() const { return integer_size; }
  uint64_t get_capacity() const { return capacity; }

  static IntegerAllocatorRef
  build(klee::ref<klee::Expr> _integer, const BDD::symbol_t &_out_of_space,
        uint64_t _capacity, addr_t _obj,
        const std::unordered_set<BDD::node_id_t> &_nodes) {
    return IntegerAllocatorRef(
        new IntegerAllocator(_integer, _out_of_space, _capacity, _obj, _nodes));
  }

  static IntegerAllocatorRef
  build(uint64_t _capacity, addr_t _obj,
        const std::unordered_set<BDD::node_id_t> &_nodes) {
    return IntegerAllocatorRef(new IntegerAllocator(_capacity, _obj, _nodes));
  }

  bool equals(const IntegerAllocator *other) const {
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

    auto other_casted = static_cast<const IntegerAllocator *>(other);
    return equals(other_casted);
  }

private:
  static bits_t bits_required_for_capacity(uint64_t capacity) {
    if (capacity <= 2) {
      return 1;
    }

    // E.g. if we are look for how many bits we need to
    // accomodate 65536 integers, we actually need
    // 16 bits, not 17 bits. This accounts for this difference.
    // Capacity = 65546, then we range from 0 to 65535
    auto ceil = capacity - 1;

    bits_t required = 2;
    uint64_t lo = 0b10;
    uint64_t hi = 0b11;

    while (required < 64 && !((lo <= ceil) && (ceil <= hi))) {
      lo = lo << 1;
      hi = (hi << 1) | 1;
      required++;
    }

    assert(lo <= ceil);
    assert(ceil <= hi);

    return required;
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
