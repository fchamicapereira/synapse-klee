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
  float fraction_of_traffic_recirculated;
  float recirculation_surplus;

public:
  TofinoContext(TNAVersion version);
  TofinoContext(const TofinoContext &other);

  ~TofinoContext();

  virtual TargetContext *clone() const override {
    return new TofinoContext(*this);
  }

  virtual int estimate_throughput_kpps() const override;

  const TNA &get_tna() const { return tna; }
  TNA &get_mutable_tna() { return tna; }

  const std::vector<DS *> &get_ds(addr_t addr) const;
  const DS *get_ds_from_id(DS_ID id) const;
  void save_ds(addr_t addr, DS *ds);

  int inc_fraction_of_traffic_recirculated(float new_fraction) {
    return fraction_of_traffic_recirculated += new_fraction;
  }

  int inc_recirculation_surplus(float surplus) {
    return recirculation_surplus += surplus;
  }

  void parser_transition(const EP *ep, const bdd::Node *node,
                         klee::ref<klee::Expr> hdr);

  void parser_select(const EP *ep, const bdd::Node *node,
                     klee::ref<klee::Expr> field,
                     const std::vector<int> &values);

  void parser_accept(const EP *ep, const bdd::Node *node);
  void parser_reject(const EP *ep, const bdd::Node *node);

  std::unordered_set<DS_ID> get_stateful_deps(const EP *ep) const;

  void place(EP *ep, addr_t obj, DS *ds, const std::unordered_set<DS_ID> &deps);
  void place_many(EP *ep, addr_t obj,
                  const std::vector<std::unordered_set<DS *>> &ds,
                  const std::unordered_set<DS_ID> &deps);

  bool check_placement(const EP *ep, const DS *ds,
                       const std::unordered_set<DS_ID> &deps) const;
  bool check_many_placements(const EP *ep,
                             const std::vector<std::unordered_set<DS *>> &ds,
                             const std::unordered_set<DS_ID> &deps) const;
};

} // namespace tofino
} // namespace synapse