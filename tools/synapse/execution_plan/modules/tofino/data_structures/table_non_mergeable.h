#pragma once

#include <unordered_set>
#include <vector>

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "data_structure.h"

namespace synapse {
namespace targets {
namespace tofino {

class TableNonMergeable;
typedef std::shared_ptr<TableNonMergeable> TableNonMergeableRef;

class TableNonMergeable : public Table {
public:
  TableNonMergeable(const std::vector<Table::key_t> &_keys,
                    const std::vector<Table::param_t> &_params,
                    const std::vector<BDD::symbol_t> &_hit,
                    const std::unordered_set<obj_addr_t> &_objs,
                    const std::unordered_set<BDD::node_id_t> &_nodes)
      : Table("non_mergeable_table", _keys, _params, _hit, _objs, _nodes) {
    type = TABLE_NON_MERGEABLE;
  }

  bool can_merge(const Table *other) const override { return false; }
  void merge(const Table *other) override {}

  static TableRef build(const std::vector<key_t> &_keys,
                        const std::vector<param_t> &_params,
                        const std::vector<BDD::symbol_t> &_hit,
                        const std::unordered_set<obj_addr_t> &_objs,
                        const std::unordered_set<BDD::node_id_t> &_nodes) {
    return TableRef(new TableNonMergeable(_keys, _params, _hit, _objs, _nodes));
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
