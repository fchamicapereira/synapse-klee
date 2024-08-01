#pragma once

#include "data_structure.h"
#include "../../../log.h"

#include "table.h"
#include "register.h"

#include <vector>
#include <optional>

namespace synapse {
namespace tofino {

struct TTLCachedTable : public DS {
  int cache_capacity;
  int num_entries;
  std::vector<bits_t> keys;

  std::vector<Table> tables;
  Register cache_expirator;
  std::vector<Register> cache_keys;

  TTLCachedTable(const TNAProperties &properties, DS_ID id, int cache_capacity,
                 int num_entries, const std::vector<bits_t> &keys);

  TTLCachedTable(const TTLCachedTable &other);

  DS *clone() const override;
  void log_debug() const override;

  std::vector<std::unordered_set<const DS *>> get_internal_ds() const;
};

} // namespace tofino
} // namespace synapse
