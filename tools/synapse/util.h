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
build_vector_modifications(const bdd::Call *vector_borrow,
                           const bdd::Call *vector_return);
std::vector<modification_t>
build_hdr_modifications(const bdd::Call *packet_borrow_next_chunk,
                        const bdd::Call *packet_return_chunk);

std::vector<modification_t>
ignore_checksum_modifications(const std::vector<modification_t> &modifications);

symbol_t create_symbol(const std::string &label, bits_t size);

bool query_contains_map_has_key(const bdd::Branch *node);

const bdd::Call *
packet_borrow_from_return(const EP *ep, const bdd::Call *packet_return_chunk);

std::vector<const bdd::Call *>
get_prev_functions(const EP *ep, const bdd::Node *node,
                   const std::vector<std::string> &functions_names,
                   bool ignore_targets = false);

std::vector<const bdd::Call *>
get_future_functions(const bdd::Node *root,
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
bool is_parser_condition(const bdd::Branch *node);

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
bool is_vector_read(const bdd::Call *vector_borrow);

std::vector<const bdd::Call *>
get_future_vector_return(const bdd::Call *vector_borrow);

// Get the data associated with this address.
klee::ref<klee::Expr> get_expr_from_addr(const EP *ep, addr_t addr);

struct map_coalescing_data_t {
  addr_t map;
  addr_t dchain;
  addr_t vector_key;
  objs_t vectors_values;
};

bool get_map_coalescing_data(const bdd::BDD *bdd, addr_t obj,
                             map_coalescing_data_t &data);
bool get_map_coalescing_data_from_dchain_op(
    const EP *ep, const bdd::Call *dchain_op,
    map_coalescing_data_t &coalescing_data);
bool get_map_coalescing_data_from_map_op(
    const EP *ep, const bdd::Call *map_op,
    map_coalescing_data_t &coalescing_data);

std::vector<const bdd::Call *>
get_coalescing_nodes_from_key(const bdd::BDD *bdd, const bdd::Node *node,
                              klee::ref<klee::Expr> key,
                              const map_coalescing_data_t &data);

klee::ref<klee::Expr> get_chunk_from_borrow(const bdd::Node *node);
bool borrow_has_var_len(const bdd::Node *node);

symbols_t get_prev_symbols(const bdd::Node *node,
                           const bdd::nodes_t &stop_nodes = bdd::nodes_t());

struct rw_fractions_t {
  double read;
  double write;
};

rw_fractions_t get_cond_map_put_rw_profile_fractions(const EP *ep,
                                                     const bdd::Node *map_get);

// Tries to find the pattern of a map_get followed by map_puts, but only when
// the map_get is not successful (i.e. the key is not found).
// Conditions to meet:
// (1) Has at least 1 future map_put
// (2) All map_put happen if the map_get was not successful
// (3) All map_puts with the target obj also have the same key as the map_get
// (4) All map_puts with the target obj update with the same value
bool is_map_get_followed_by_map_puts_on_miss(
    const bdd::BDD *bdd, const bdd::Call *map_get,
    std::vector<const bdd::Call *> &map_puts);

// (1) Has at least 1 future map_put
// (2) All map_put happen if the dchain_allocate_new_index was successful
// (3) All map_puts with the target obj also have the same key as the map_get
// (4) All map_puts with the target obj update with the same value
bool is_map_update_with_dchain(const EP *ep,
                               const bdd::Call *dchain_allocate_new_index,
                               std::vector<const bdd::Call *> &map_puts);

// Tries to find the pattern of a map_get followed by map_erases, but only when
// the map_get is successful (i.e. the key is found).
// Conditions to meet:
// (1) Has at least 1 future map_erase
// (2) All map_erase happen if the map_get was successful
// (3) All map_erases with the target obj also have the same key as the map_get
bool is_map_get_followed_by_map_erases_on_hit(
    const bdd::BDD *bdd, const bdd::Call *map_get,
    std::vector<const bdd::Call *> &map_erases);

// Appends new non-branch nodes to the BDD in place of the provided current
// node.
// Clones all new_nodes and appends them to the BDD.
void add_non_branch_nodes_to_bdd(
    const EP *ep, bdd::BDD *bdd, const bdd::Node *current,
    const std::vector<const bdd::Node *> &new_nodes, bdd::Node *&new_current);

// Appends a single new branch node to the BDD in place of the provided current
// node. This duplicates the BDD portion starting from the current node, and
// appends the cloned portion to one of the branches.
void add_branch_to_bdd(const EP *ep, bdd::BDD *bdd, const bdd::Node *current,
                       klee::ref<klee::Expr> condition,
                       bdd::Branch *&new_branch);

void delete_non_branch_node_from_bdd(const EP *ep, bdd::BDD *bdd,
                                     bdd::node_id_t target_id,
                                     bdd::Node *&new_current);

void delete_branch_node_from_bdd(const EP *ep, bdd::BDD *bdd,
                                 bdd::node_id_t target_id,
                                 bool direction_to_keep,
                                 bdd::Node *&new_current);

const bdd::Branch *
find_branch_checking_index_alloc(const EP *ep, const bdd::Node *node,
                                 const symbol_t &out_of_space);

std::string int2hr(uint64_t value);
std::string throughput2str(uint64_t thpt, const std::string &units,
                           bool human_readable = false);

} // namespace synapse