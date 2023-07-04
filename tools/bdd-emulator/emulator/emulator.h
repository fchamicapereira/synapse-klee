#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include <unordered_map>
#include <vector>

#include "../common.h"
#include "data_structures/data_structures.h"
#include "internals/internals.h"
#include "operations/operations.h"
#include "reporter.h"

namespace BDD {
namespace emulation {

class Emulator {
private:
  const BDD &bdd;
  cfg_t cfg;

  state_t state;
  meta_t meta;

  operations_t operations;
  Reporter reporter;

public:
  Emulator(const BDD &_bdd, cfg_t _cfg)
      : bdd(_bdd), cfg(_cfg), operations(get_operations()),
        reporter(bdd, meta, cfg.warmup) {
    kutil::solver_toolbox.build();
    setup();
  }

  void list_operations() const;

  void run(pkt_t pkt, time_ns_t time, uint16_t device);
  void run(const std::string &pcap_file, uint16_t device);

  const meta_t &get_meta() { return meta; }
  const Reporter &get_reporter() const { return reporter; }

private:
  void dump_context(const context_t &ctx) const;
  bool evaluate_condition(klee::ref<klee::Expr> condition,
                          context_t &ctx) const;
  operation_ptr get_operation(const std::string &name) const;
  void process(Node_ptr node, pkt_t pkt, time_ns_t time, context_t &ctx);
  void setup();
};

} // namespace emulation
} // namespace BDD