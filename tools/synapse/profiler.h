#pragma once

#include "call-paths-to-bdd.h"
#include "bdd-analyzer-report.h"

#include <optional>

namespace synapse {

typedef std::vector<klee::ref<klee::Expr>> constraints_t;

struct FlowStats {
  klee::ref<klee::Expr> flow_id;
  uint64_t total_packets;
  uint64_t total_flows;
  uint64_t avg_pkts_per_flow;
  std::vector<uint64_t> packets_per_flow;
};

struct ProfilerNode {
  klee::ref<klee::Expr> constraint;
  double fraction;
  std::optional<bdd::node_id_t> bdd_node_id;
  std::vector<FlowStats> flows_stats;

  ProfilerNode *on_true;
  ProfilerNode *on_false;
  ProfilerNode *prev;

  ProfilerNode(klee::ref<klee::Expr> _constraint, double _fraction);
  ProfilerNode(klee::ref<klee::Expr> _constraint, double _fraction,
               bdd::node_id_t _bdd_node_id);
  ~ProfilerNode();

  ProfilerNode *clone(bool keep_bdd_info) const;
  void log_debug(int lvl = 0) const;

  bool get_flow_stats(klee::ref<klee::Expr> flow_id,
                      FlowStats &flow_stats) const;
};

class Profiler {
private:
  bdd_profile_t bdd_profile;
  ProfilerNode *root;

public:
  Profiler(const bdd::BDD *bdd, const bdd_profile_t &_bdd_profile);
  Profiler(const bdd::BDD *bdd, const std::string &_bdd_profile_fname);
  Profiler(const bdd::BDD *bdd);

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
  std::optional<FlowStats> get_flow_stats(const constraints_t &constraints,
                                          klee::ref<klee::Expr> flow_id) const;

  void log_debug() const;

private:
  ProfilerNode *get_node(const constraints_t &constraints) const;

  void append(ProfilerNode *node, klee::ref<klee::Expr> constraint,
              double fraction);
  void remove(ProfilerNode *node);
  void replace_root(klee::ref<klee::Expr> constraint, double fraction);
};

} // namespace synapse