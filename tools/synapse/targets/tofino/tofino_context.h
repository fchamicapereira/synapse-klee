#pragma once

#include "../context.h"
#include "tna/tna.h"
#include "data_structures/data_structures.h"

#include <unordered_map>
#include <optional>

namespace synapse {
namespace tofino {

class TofinoContext : public TargetContext {
private:
  TNA tna;
  std::unordered_map<addr_t, std::vector<DS *>> obj_to_ds;
  std::unordered_map<DS_ID, DS *> id_to_ds;

public:
  TofinoContext(TNAVersion version, const Profiler *profiler);
  TofinoContext(const TofinoContext &other);

  ~TofinoContext();

  virtual TargetContext *clone() const override {
    return new TofinoContext(*this);
  }

  virtual uint64_t estimate_throughput_pps() const override;

  const TNA &get_tna() const { return tna; }
  TNA &get_mutable_tna() { return tna; }

  const std::vector<DS *> &get_ds(addr_t addr) const;
  const DS *get_ds_from_id(DS_ID id) const;
  void save_ds(addr_t addr, DS *ds);

  void add_recirculated_traffic(int port, int port_recirculations,
                                double fraction,
                                std::optional<int> prev_recirc_port);

  void parser_transition(const EP *ep, const bdd::Node *node,
                         klee::ref<klee::Expr> hdr);

  void parser_select(const EP *ep, const bdd::Node *node,
                     klee::ref<klee::Expr> field,
                     const std::vector<int> &values);

  void parser_accept(const EP *ep, const bdd::Node *node);
  void parser_reject(const EP *ep, const bdd::Node *node);

  std::unordered_set<DS_ID> get_stateful_deps(const EP *ep,
                                              const bdd::Node *node) const;

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