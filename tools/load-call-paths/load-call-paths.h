#pragma once

#include <unordered_map>
#include <unordered_set>

#include "klee/Constraints.h"
#include "klee/ExprBuilder.h"

typedef uint32_t bits_t;

struct meta_t {
  std::string symbol;
  bits_t offset;
  bits_t size;
};

struct arg_t {
  klee::ref<klee::Expr> expr;
  std::pair<bool, std::string> fn_ptr_name;
  klee::ref<klee::Expr> in;
  klee::ref<klee::Expr> out;
  std::vector<meta_t> meta;
};

typedef std::pair<klee::ref<klee::Expr>, klee::ref<klee::Expr>> extra_var_t;

struct call_t {
  std::string function_name;
  std::unordered_map<std::string, extra_var_t> extra_vars;
  std::unordered_map<std::string, arg_t> args;
  klee::ref<klee::Expr> ret;
};

typedef std::vector<call_t> calls_t;

struct symbol_t {
  std::string base;
  const klee::Array *array;
  klee::ref<klee::Expr> expr;
};

struct symbol_hash_t {
  std::size_t operator()(const symbol_t &s) const noexcept {
    return s.array->hash();
  }
};

struct symbol_equal_t {
  bool operator()(const symbol_t &a, const symbol_t &b) const noexcept {
    return a.array->name == b.array->name;
  }
};

typedef std::unordered_set<symbol_t, symbol_hash_t, symbol_equal_t> symbols_t;

bool get_symbol(const symbols_t &symbols, const std::string &base,
                symbol_t &symbol);

struct call_path_t {
  std::string file_name;
  klee::ConstraintManager constraints;
  symbols_t symbols;
  calls_t calls;
};

call_path_t *load_call_path(const std::string &file_name);

struct call_paths_t {
  std::vector<call_path_t *> cps;

  call_paths_t() {}

  call_paths_t(const std::vector<std::string> &call_path_files) {
    for (const std::string &file : call_path_files)
      cps.push_back(load_call_path(file));
    merge_symbols();
  }

  symbols_t get_symbols() const {
    symbols_t symbols;
    for (const call_path_t *cp : cps)
      symbols.insert(cp->symbols.begin(), cp->symbols.end());
    return symbols;
  }

  void merge_symbols();

  void free_call_paths() {
    for (call_path_t *cp : cps)
      delete cp;
  }
};
