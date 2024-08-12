#include "fcfs_cached_table.h"

namespace synapse {
namespace tofino {

static bits_t index_size_from_cache_capacity(int cache_capacity) {
  // Log base 2 of the cache capacity
  // Assert cache capacity is a power of 2
  assert((cache_capacity & (cache_capacity - 1)) == 0);
  return bits_t(log2(cache_capacity));
}

static Table build_table(DS_ID id, int num_entries,
                         const std::vector<bits_t> &keys) {
  return Table(id + "_table", num_entries, keys, {});
}

static Register build_cache_expirator(const TNAProperties &properties, DS_ID id,
                                      int cache_capacity) {
  bits_t hash_size = index_size_from_cache_capacity(cache_capacity);
  bits_t timestamp_size = 32;
  return Register(properties, id + "_expirator", cache_capacity, hash_size,
                  timestamp_size, {RegisterAction::WRITE});
}

static std::vector<Register> build_cache_keys(const TNAProperties &properties,
                                              DS_ID id,
                                              const std::vector<bits_t> &keys,
                                              int cache_capacity) {
  std::vector<Register> cache_keys;

  bits_t hash_size = index_size_from_cache_capacity(cache_capacity);

  int i = 0;
  for (bits_t key : keys) {
    bits_t cell_size = key;
    Register cache_key(properties, id + "_key_" + std::to_string(i),
                       cache_capacity, hash_size, cell_size,
                       {RegisterAction::READ, RegisterAction::SWAP});
    i++;
    cache_keys.push_back(cache_key);
  }

  return cache_keys;
}

FCFSCachedTable::FCFSCachedTable(const TNAProperties &properties, DS_ID _id,
                                 int _cache_capacity, int _num_entries,
                                 const std::vector<bits_t> &_keys)
    : DS(DSType::CACHED_TABLE, _id), cache_capacity(_cache_capacity),
      num_entries(_num_entries), keys(_keys),
      tables({
          build_table(id + "_0", num_entries, keys),
          build_table(id + "_1", num_entries, keys),
          build_table(id + "_2", num_entries, keys),
      }),
      cache_expirator(build_cache_expirator(properties, _id, cache_capacity)),
      cache_keys(build_cache_keys(properties, id, keys, cache_capacity)) {}

FCFSCachedTable::FCFSCachedTable(const FCFSCachedTable &other)
    : DS(DSType::CACHED_TABLE, other.id), cache_capacity(other.cache_capacity),
      num_entries(other.num_entries), keys(other.keys), tables(other.tables),
      cache_expirator(other.cache_expirator), cache_keys(other.cache_keys) {}

DS *FCFSCachedTable::clone() const { return new FCFSCachedTable(*this); }

void FCFSCachedTable::log_debug() const {
  Log::dbg() << "\n";
  Log::dbg() << "======== CACHED TABLE ========\n";
  Log::dbg() << "ID:      " << id << "\n";
  Log::dbg() << "Entries: " << num_entries << "\n";
  Log::dbg() << "Cache:   " << cache_capacity << "\n";
  for (const Table &table : tables) {
    table.log_debug();
  }
  cache_expirator.log_debug();
  for (const Register &cache_key : cache_keys) {
    cache_key.log_debug();
  }
  Log::dbg() << "==============================\n";
}

std::vector<std::unordered_set<const DS *>>
FCFSCachedTable::get_internal_ds() const {
  // Access to the table comes first, then the expirator, and finally the keys.

  std::vector<std::unordered_set<const DS *>> internal_ds;

  internal_ds.emplace_back();
  for (const Table &table : tables)
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