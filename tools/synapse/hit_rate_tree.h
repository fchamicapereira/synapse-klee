#pragma once

#include "call-paths-to-bdd.h"

#include <optional>

namespace synapse {

typedef std::vector<klee::ref<klee::Expr>> constraints_t;

struct HitRateNode {
  klee::ref<klee::Expr> constraint;
  float fraction;
  std::optional<bdd::node_id_t> bdd_node_id;

  HitRateNode *on_true;
  HitRateNode *on_false;
  HitRateNode *prev;

  HitRateNode(klee::ref<klee::Expr> _constraint, float _fraction);
  HitRateNode(klee::ref<klee::Expr> _constraint, float _fraction,
              bdd::node_id_t _bdd_node_id);
  ~HitRateNode();

  HitRateNode *clone() const;
  void dump(int lvl = 0) const;
};

class HitRateTree {
private:
  HitRateNode *root;

public:
  HitRateTree(const bdd::BDD *bdd, unsigned random_seed);
  HitRateTree(const bdd::BDD *bdd, const std::string &bdd_profile_fname);

  HitRateTree(const HitRateTree &other);
  HitRateTree(HitRateTree &&other);

  ~HitRateTree();

  void insert(const constraints_t &constraints,
              klee::ref<klee::Expr> constraint, float fraction_on_true);
  void insert_relative(const constraints_t &constraints,
                       klee::ref<klee::Expr> constraint,
                       float rel_fraction_on_true);

  std::optional<float>
  get_fraction(const constraints_t &ordered_constraints) const;
  void dump() const;

private:
  HitRateNode *get_node(const constraints_t &constraints) const;
  void append(HitRateNode *node, klee::ref<klee::Expr> constraint,
              float fraction);
};

} // namespace synapse