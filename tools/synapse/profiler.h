#pragma once

#include "call-paths-to-bdd.h"

#include <optional>

namespace synapse {

typedef std::vector<klee::ref<klee::Expr>> constraints_t;

struct ProfilerNode {
  klee::ref<klee::Expr> constraint;
  double fraction;
  std::optional<bdd::node_id_t> bdd_node_id;

  ProfilerNode *on_true;
  ProfilerNode *on_false;
  ProfilerNode *prev;

  ProfilerNode(klee::ref<klee::Expr> _constraint, double _fraction);
  ProfilerNode(klee::ref<klee::Expr> _constraint, double _fraction,
               bdd::node_id_t _bdd_node_id);
  ~ProfilerNode();

  ProfilerNode *clone(bool keep_bdd_info) const;
  void log_debug(int lvl = 0) const;
};

class Profiler {
private:
  ProfilerNode *root;
  int avg_pkt_bytes;

public:
  Profiler(const bdd::BDD *bdd, unsigned random_seed);
  Profiler(const bdd::BDD *bdd, const std::string &bdd_profile_fname);

  Profiler(const Profiler &other);
  Profiler(Profiler &&other);

  ~Profiler();

  int get_avg_pkt_bytes() const;

  void insert(const constraints_t &constraints,
              klee::ref<klee::Expr> constraint, double fraction_on_true);
  void insert_relative(const constraints_t &constraints,
                       klee::ref<klee::Expr> constraint,
                       double rel_fraction_on_true);
  void remove(const constraints_t &constraints);
  void scale(const constraints_t &constraints, double factor);

  const ProfilerNode *get_root() const { return root; }
  std::optional<double> get_fraction(const constraints_t &constraints) const;
  void log_debug() const;

private:
  ProfilerNode *get_node(const constraints_t &constraints) const;

  void append(ProfilerNode *node, klee::ref<klee::Expr> constraint,
              double fraction);
  void remove(ProfilerNode *node);
  void replace_root(klee::ref<klee::Expr> constraint, double fraction);
};

} // namespace synapse