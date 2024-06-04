#pragma once

#include "../context.h"
#include "tna/tna.h"
#include "data_structures/data_structures.h"

#include <unordered_map>

namespace synapse {
namespace tofino {

class TofinoContext : public TargetContext {
private:
  TNA tna;
  std::unordered_map<addr_t, std::vector<DS *>> data_structures;
  std::unordered_set<DS_ID> ids;

public:
  TofinoContext(TNAVersion version);
  TofinoContext(const TofinoContext &other);

  ~TofinoContext();

  virtual TargetContext *clone() const override {
    return new TofinoContext(*this);
  }

  const TNA &get_tna() const { return tna; }
  TNA &get_mutable_tna() { return tna; }

  const std::vector<DS *> &get_ds(addr_t addr) const;
  std::unordered_set<const DS *> get_prev_ds(const EP *ep, DSType type) const;
  void save_ds(addr_t addr, DS *ds);

  const Table *get_table(int tid) const;

  void parser_transition(const EP *ep, const bdd::Node *node,
                         klee::ref<klee::Expr> hdr);

  void parser_select(const EP *ep, const bdd::Node *node,
                     klee::ref<klee::Expr> field,
                     const std::vector<int> &values);

  void parser_accept(const EP *ep, const bdd::Node *node);
  void parser_reject(const EP *ep, const bdd::Node *node);

  std::unordered_set<DS_ID> get_table_dependencies(const EP *ep) const;
  void save_table(EP *ep, addr_t obj, Table *table,
                  const std::unordered_set<DS_ID> &deps);
  bool check_table_placement(const EP *ep, const Table *table,
                             const std::unordered_set<DS_ID> &deps) const;
};

} // namespace tofino
} // namespace synapse