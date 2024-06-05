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
  std::unordered_map<addr_t, std::vector<DS *>> obj_to_ds;
  std::unordered_map<DS_ID, DS *> id_to_ds;

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
  const DS *get_ds_from_id(DS_ID id) const;
  void save_ds(addr_t addr, DS *ds);

  void parser_transition(const EP *ep, const bdd::Node *node,
                         klee::ref<klee::Expr> hdr);

  void parser_select(const EP *ep, const bdd::Node *node,
                     klee::ref<klee::Expr> field,
                     const std::vector<int> &values);

  void parser_accept(const EP *ep, const bdd::Node *node);
  void parser_reject(const EP *ep, const bdd::Node *node);

  std::unordered_set<DS_ID> get_stateful_deps(const EP *ep) const;

  void save_table(EP *ep, addr_t obj, Table *table,
                  const std::unordered_set<DS_ID> &deps);
  bool check_table_placement(const EP *ep, const Table *table,
                             const std::unordered_set<DS_ID> &deps) const;

  void save_register(EP *ep, addr_t obj, Register *reg,
                     const std::unordered_set<DS_ID> &deps);
  bool check_register_placement(const EP *ep, const Register *reg,
                                const std::unordered_set<DS_ID> &deps) const;
};

} // namespace tofino
} // namespace synapse