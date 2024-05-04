#pragma once

#include "../../memory_bank.h"
#include "klee-util.h"

namespace synapse {
namespace targets {
namespace x86_tofino {

class x86TofinoMemoryBank : public TargetMemoryBank {
public:
  struct expiration_t {
    bool set;
    addr_t chain;
    addr_t map;
    addr_t vector;
    klee::ref<klee::Expr> time;

    expiration_t() : set(false) {}
    expiration_t(addr_t _chain, addr_t _map, addr_t _vector,
                 klee::ref<klee::Expr> _time)
        : set(true), chain(_chain), map(_map), vector(_vector), time(_time) {}
  };

  struct vector_borrow_t {
    addr_t vector_addr;
    klee::ref<klee::Expr> value_out;
  };

  enum ds_type_t {
    MAP,
    DCHAIN,
  };

  struct ds_t {
    ds_type_t type;
    addr_t addr;
    BDD::node_id_t node_id;

    ds_t(ds_type_t _type, addr_t _addr, BDD::node_id_t _node_id)
        : type(_type), addr(_addr), node_id(_node_id) {}

    bool matches(ds_type_t _type, addr_t _addr) const {
      if (type != _type) {
        return false;
      }

      return matches(_addr);
    }

    bool matches(addr_t _addr) const {
      if (addr != _addr) {
        return false;
      }

      return true;
    }
  };

  struct map_t : ds_t {
    bits_t value_size;

    map_t(addr_t _addr, bits_t _value_size, BDD::node_id_t _node_id)
        : ds_t(ds_type_t::MAP, _addr, _node_id), value_size(_value_size) {}
  };

  struct dchain_t : ds_t {
    uint64_t index_range;

    dchain_t(addr_t _addr, BDD::node_id_t _node_id, uint64_t _index_range)
        : ds_t(ds_type_t::DCHAIN, _addr, _node_id), index_range(_index_range) {}
  };

private:
  std::vector<BDD::symbol_t> time;
  expiration_t expiration;
  std::vector<vector_borrow_t> vector_borrows;
  std::vector<std::shared_ptr<ds_t>> data_structures;

public:
  x86TofinoMemoryBank() {}

  x86TofinoMemoryBank(const x86TofinoMemoryBank &mb)
      : time(mb.time), expiration(mb.expiration),
        vector_borrows(mb.vector_borrows), data_structures(mb.data_structures) {
  }

  void add_time(BDD::symbol_t _time) { time.push_back(_time); }
  const std::vector<BDD::symbol_t> &get_time() const { return time; }

  void set_expiration(const expiration_t &_expiration) {
    expiration = _expiration;
  }

  expiration_t get_expiration() const { return expiration; }

  bool has_data_structure(addr_t addr) const {
    for (auto ds : data_structures) {
      if (ds->matches(addr)) {
        return true;
      }
    }

    return false;
  }

  void add_data_structure(std::shared_ptr<ds_t> ds) {
    assert(!has_data_structure(ds->addr));
    data_structures.push_back(ds);
  }

  const std::vector<std::shared_ptr<ds_t>> &get_data_structures() const {
    return data_structures;
  }

  virtual TargetMemoryBank_ptr clone() const override {
    auto clone = new x86TofinoMemoryBank(*this);
    return TargetMemoryBank_ptr(clone);
  }
};

} // namespace x86_tofino
} // namespace targets
} // namespace synapse