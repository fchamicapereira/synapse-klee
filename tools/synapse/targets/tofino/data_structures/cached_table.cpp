#include "cached_table.h"

namespace synapse {
namespace tofino {

static bits_t index_size_from_cache_capacity(int cache_capacity) {
  // Log base 2 of the cache capacity
  // Assert cache capacity is a power of 2
  assert((cache_capacity & (cache_capacity - 1)) == 0);
  return bits_t(log2(cache_capacity));
}

static Table build_table(DS_ID id, int num_entries,
                         const std::vector<bits_t> &keys, bits_t value) {
  return Table(id + "_table", num_entries, keys, {value});
}

static Register build_cache_expirator(const TNAConstraints &constraints,
                                      DS_ID id, int cache_capacity) {
  bits_t hash_size = index_size_from_cache_capacity(cache_capacity);
  bits_t timestamp_size = 32;
  return Register(constraints, id + "_expirator", cache_capacity, hash_size,
                  timestamp_size, {RegisterAction::WRITE});
}

static std::vector<Register> build_cache_keys(const TNAConstraints &constraints,
                                              DS_ID id,
                                              const std::vector<bits_t> &keys,
                                              int cache_capacity) {
  std::vector<Register> cache_keys;

  bits_t hash_size = index_size_from_cache_capacity(cache_capacity);

  int i = 0;
  for (bits_t key : keys) {
    bits_t cell_size = key;
    Register cache_key(constraints, id + "_key_" + std::to_string(i),
                       cache_capacity, hash_size, cell_size,
                       {RegisterAction::READ, RegisterAction::SWAP});
    i++;
    cache_keys.push_back(cache_key);
  }

  return cache_keys;
}

CachedTable::CachedTable(const TNAConstraints &constraints, DS_ID _id,
                         int _cache_capacity, int _num_entries,
                         const std::vector<bits_t> &_keys, bits_t _value)
    : DS(DSType::CACHED_TABLE, _id), cache_capacity(_cache_capacity),
      num_entries(_num_entries), keys(_keys), value(_value),
      table(build_table(id, num_entries, keys, value)),
      cache_expirator(build_cache_expirator(constraints, _id, cache_capacity)),
      cache_keys(build_cache_keys(constraints, id, keys, cache_capacity)) {}

CachedTable::CachedTable(const CachedTable &other)
    : DS(DSType::CACHED_TABLE, other.id), cache_capacity(other.cache_capacity),
      num_entries(other.num_entries), keys(other.keys), value(other.value),
      table(other.table), cache_expirator(other.cache_expirator),
      cache_keys(other.cache_keys) {}

DS *CachedTable::clone() const { return new CachedTable(*this); }

void CachedTable::log_debug() const {
  Log::dbg() << "\n";
  Log::dbg() << "======== CACHED TABLE ========\n";
  Log::dbg() << "ID:      " << id << "\n";
  Log::dbg() << "Entries: " << num_entries << "\n";
  Log::dbg() << "Cache:   " << cache_capacity << "\n";
  table.log_debug();
  cache_expirator.log_debug();
  for (const Register &cache_key : cache_keys) {
    cache_key.log_debug();
  }
  Log::dbg() << "==============================\n";
}

std::vector<std::unordered_set<const DS *>>
CachedTable::get_internal_ds() const {
  // Access to the table comes first, then the expirator, and finally the keys.

  std::vector<std::unordered_set<const DS *>> internal_ds;

  internal_ds.emplace_back();
  internal_ds.back().insert(&table);

  internal_ds.emplace_back();
  internal_ds.back().insert(&cache_expirator);

  internal_ds.emplace_back();
  for (const Register &cache_key : cache_keys)
    internal_ds.back().insert(&cache_key);

  return internal_ds;
}

} // namespace tofino
} // namespace synapse