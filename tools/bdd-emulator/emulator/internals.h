#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "data_structures/byte.h"

#include <assert.h>
#include <unordered_map>

namespace BDD {
namespace emulation {

typedef float gbps_t;
typedef uint64_t time_us_t;
typedef uint64_t time_ns_t;
typedef float hit_rate_t;

class DataStructure;
typedef std::shared_ptr<DataStructure> DataStructureRef;

struct state_t {
  std::unordered_map<addr_t, DataStructureRef> data_structures;

  void add(const DataStructureRef &ds);
  DataStructureRef get(addr_t obj) const;
};

typedef klee::ConstraintManager context_t;

inline void concretize(context_t &ctx, klee::ref<klee::Expr> expr,
                       uint64_t value) {
  assert(expr->getWidth() <= 64);
  auto value_expr =
      kutil::solver_toolbox.exprBuilder->Constant(value, expr->getWidth());
  auto concretize_expr =
      kutil::solver_toolbox.exprBuilder->Eq(expr, value_expr);
  ctx.addConstraint(concretize_expr);
}

inline void concretize(context_t &ctx, klee::ref<klee::Expr> expr,
                       const uint8_t *value) {
  auto width = expr->getWidth();

  auto byte = value[0];
  auto value_expr = kutil::solver_toolbox.exprBuilder->Constant(byte, 8);

  for (auto b = 1u; b < width / 8; b++) {
    auto byte = value[b];
    auto byte_expr = kutil::solver_toolbox.exprBuilder->Constant(byte, 8);
    value_expr =
        kutil::solver_toolbox.exprBuilder->Concat(byte_expr, value_expr);
  }

  auto concretize_expr =
      kutil::solver_toolbox.exprBuilder->Eq(expr, value_expr);
  ctx.addConstraint(concretize_expr);
}

inline bytes_t bytes_from_expr(klee::ref<klee::Expr> expr, const context_t& ctx) {
  auto size = expr->getWidth() / 8;
  assert(expr->getWidth() % 8 == 0);

  bytes_t values(size);

  for (auto byte = 0u; byte < size; byte++) {
    auto byte_expr =
        kutil::solver_toolbox.exprBuilder->Extract(expr, byte * 8, 8);
    auto byte_value = kutil::solver_toolbox.value_from_expr(byte_expr, ctx);
    values[size - byte - 1] = byte_value;
  }

  return values;
}

struct meta_t {
  std::unordered_map<node_id_t, uint64_t> hit_counter;
  uint64_t packet_counter;
  uint64_t accepted;
  uint64_t rejected;
  time_ns_t elapsed_time;
  uint64_t flows_expired;
  uint64_t dchain_allocations;

  meta_t() { reset(); }

  void reset() {
    hit_counter.clear();
    packet_counter = 0;
    accepted = 0;
    rejected = 0;
    elapsed_time = 0;
    flows_expired = 0;
    dchain_allocations = 0;
  }

  std::unordered_map<node_id_t, emulation::hit_rate_t> get_hit_rate() const;
};

inline std::ostream &operator<<(std::ostream &os, const meta_t &meta) {
  auto hit_rate = meta.get_hit_rate();

  auto churn_fpm =
      (60.0 * meta.flows_expired) / (double)(meta.elapsed_time * 1e-9);

  os << "Meta:\n";
  os << "  packets:       " << meta.packet_counter << "\n";
  os << "  accepted:      " << meta.accepted << " ("
     << (100.0 * meta.accepted / meta.packet_counter) << "%)\n";
  os << "  rejected:      " << meta.rejected << " ("
     << (100.0 * meta.rejected / meta.packet_counter) << "%)\n";
  os << "  elapsed time:  " << meta.elapsed_time << " ns ("
     << (double)(meta.elapsed_time) * 1e-9 << " s)\n";
  os << "  flows expired: " << meta.flows_expired << " (" << churn_fpm
     << " fpm)\n";
  os << "  dchain allocs: " << meta.dchain_allocations << "\n";

  os << "Hit rate (per node):\n";
  for (auto it = hit_rate.begin(); it != hit_rate.end(); it++) {
    os << "  " << it->first << " \t " << 100 * it->second << "%\n";
  }

  return os;
}

struct cfg_t {
  std::pair<bool, gbps_t> rate;
  std::pair<bool, time_us_t> timeout;
  std::pair<bool, int> loop;
  bool warmup;
};

struct pkt_t {
  const uint8_t *data;
  uint32_t size;
};

typedef void (*operation_ptr)(const Call *call, pkt_t &pkt, time_ns_t time,
                              state_t &state, meta_t &meta, context_t &ctx,
                              const cfg_t &cfg);

typedef std::map<std::string, operation_ptr> operations_t;

} // namespace emulation
} // namespace BDD