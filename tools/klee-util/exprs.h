#pragma once

#include "klee/ExprBuilder.h"
#include "klee/util/ExprSMTLIBPrinter.h"
#include "klee/util/ExprVisitor.h"

#include <iostream>
#include <unordered_set>

typedef uint32_t bytes_t;
typedef uint64_t addr_t;

namespace kutil {

bool get_bytes_read(klee::ref<klee::Expr> expr, std::vector<unsigned> &bytes);
bool is_readLSB(klee::ref<klee::Expr> expr);
bool is_readLSB(klee::ref<klee::Expr> expr, std::string &symbol);
bool is_packet_readLSB(klee::ref<klee::Expr> expr);
bool is_packet_readLSB(klee::ref<klee::Expr> expr, bytes_t &offset,
                       int &n_bytes);
bool is_bool(klee::ref<klee::Expr> expr);
bool is_constant(klee::ref<klee::Expr> expr);
bool is_constant_signed(klee::ref<klee::Expr> expr);
int64_t get_constant_signed(klee::ref<klee::Expr> expr);
bool manager_contains(klee::ConstraintManager constraints,
                      klee::ref<klee::Expr> expr);
klee::ConstraintManager join_managers(klee::ConstraintManager m1,
                                      klee::ConstraintManager m2);

bool has_symbols(klee::ref<klee::Expr> expr);
std::unordered_set<std::string> get_symbols(klee::ref<klee::Expr> expr);
std::pair<bool, std::string> get_symbol(klee::ref<klee::Expr> expr);
addr_t expr_addr_to_obj_addr(klee::ref<klee::Expr> obj_addr);

klee::ref<klee::Expr> simplify(klee::ref<klee::Expr> expr);
klee::ref<klee::Expr> filter(klee::ref<klee::Expr> expr,
                             const std::vector<std::string> &allowed_symbols);
klee::ref<klee::Expr> swap_packet_endianness(klee::ref<klee::Expr> expr);

} // namespace kutil