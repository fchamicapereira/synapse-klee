#pragma once

#include "../context.h"
#include "tna/tna.h"
#include "data_structures/data_structures.h"

#include <unordered_map>

namespace synapse {
namespace tofino {

class TofinoContext : public TargetContext {
private:
  const TNAVersion version;
  const std::unordered_map<TNAProperty, int> properties;

  std::unordered_map<addr_t, std::vector<DataStructure *>> data_structures;
  std::unordered_set<DataStructureID> ids;

  Parser parser;

public:
  TofinoContext(TNAVersion version);
  TofinoContext(const TofinoContext &other);

  ~TofinoContext();

  virtual TargetContext *clone() const override {
    return new TofinoContext(*this);
  }

  const std::vector<DataStructure *> &get_data_structures(addr_t addr) const;
  void add_data_structure(addr_t addr, DataStructure *ds);
  const Table *get_table(int tid) const;

  // Tofino compiler complains if we access more than 4 bytes of the packet on
  // the same if statement.
  bool condition_meets_phv_limit(klee::ref<klee::Expr> expr) const;

  void parser_transition(const EP *ep, const bdd::Node *node,
                         klee::ref<klee::Expr> hdr);

  void parser_select(const EP *ep, const bdd::Node *node,
                     klee::ref<klee::Expr> field,
                     const std::vector<int> &values);

  void parser_accept(const EP *ep, const bdd::Node *node);
  void parser_reject(const EP *ep, const bdd::Node *node);
};

} // namespace tofino
} // namespace synapse