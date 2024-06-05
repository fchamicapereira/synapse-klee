#pragma once

#include "call-paths-to-bdd.h"
#include "bdd-visualizer.h"

#include <vector>
#include <string>
#include <unordered_set>
#include <optional>

#include <stdint.h>

#define UINT_16_SWAP_ENDIANNESS(p) ((((p)&0xff) << 8) | ((p) >> 8 & 0xff))

namespace synapse {

enum class ModuleType;
class Module;
class EP;

typedef uint64_t time_ns_t;
typedef uint64_t time_ms_t;

struct modification_t {
  int byte;
  klee::ref<klee::Expr> expr;

  modification_t(int _byte, klee::ref<klee::Expr> _expr)
      : byte(_byte), expr(_expr) {}

  modification_t(const modification_t &modification)
      : byte(modification.byte), expr(modification.expr) {}
};

std::vector<modification_t> build_modifications(klee::ref<klee::Expr> before,
                                                klee::ref<klee::Expr> after);

std::vector<modification_t>
ignore_checksum_modifications(const std::vector<modification_t> &modifications);

bool query_contains_map_has_key(const bdd::Branch *node);

klee::ref<klee::Expr> chunk_borrow_from_return(const EP *ep,
                                               const bdd::Node *node);

std::vector<const bdd::Node *>
get_prev_functions(const EP *ep, const bdd::Node *node,
                   const std::vector<std::string> &functions_names);

std::vector<const bdd::Node *>
get_all_functions_after_node(const bdd::Node *root,
                             const std::vector<std::string> &functions,
                             bool stop_on_branches = false);

bool is_parser_drop(const bdd::Node *root);

// A parser condition should be the single discriminating condition that
// decides whether a parsing state is performed or not. In BDD language, it
// decides if a specific packet_borrow_next_chunk is applied.
//
// One classic example would be condition that checks if the ethertype field
// on the ethernet header equals the IP protocol.
//
// A branch condition is considered a parsing condition if:
//   - Has pending chunks to be borrowed in the future
//   - Only looks at the packet
bool is_parser_condition(const bdd::Node *node);

std::vector<const Module *>
get_prev_modules(const EP *ep, const std::vector<ModuleType> &types);

bool is_expr_only_packet_dependent(klee::ref<klee::Expr> expr);

struct counter_data_t {
  bool valid;
  std::vector<const bdd::Node *> reads;
  std::vector<const bdd::Node *> writes;
  std::optional<uint64_t> max_value;

  counter_data_t() : valid(false) {}
};

// Check if a given data structure is a counter. We expect counters to be
// implemented with vectors, such that (1) the value it stores is <= 64 bits,
// and (2) the only write operations performed on them increment the stored
// value.
counter_data_t is_counter(const EP *ep, addr_t obj);

// When we encounter a vector_return operation and want to retrieve its
// vector_borrow value counterpart. This is useful to compare changes to the
// value expression and retrieve the performed modifications (if any).
klee::ref<klee::Expr> get_original_vector_value(const EP *ep,
                                                const bdd::Node *node,
                                                addr_t target_addr);
klee::ref<klee::Expr> get_original_vector_value(const EP *ep,
                                                const bdd::Node *node,
                                                addr_t target_addr,
                                                const bdd::Node *&source);

bool is_vector_return_without_modifications(const EP *ep,
                                            const bdd::Node *node);

const bdd::Node *get_future_vector_return(const bdd::Node *root,
                                          addr_t target_addr);

// Get the data associated with this address.
klee::ref<klee::Expr> get_expr_from_addr(const EP *ep, addr_t addr);

struct map_coalescing_data_t {
  addr_t map;
  addr_t dchain;
  objs_t vectors;
};

std::optional<map_coalescing_data_t>
get_map_coalescing_data(const bdd::BDD *bdd, addr_t map_addr);

klee::ref<klee::Expr> get_chunk_from_borrow(const bdd::Node *node);
bool borrow_has_var_len(const bdd::Node *node);

symbols_t get_prev_symbols(const bdd::Node *node,
                           const bdd::nodes_t &stop_nodes = bdd::nodes_t());

} // namespace synapse