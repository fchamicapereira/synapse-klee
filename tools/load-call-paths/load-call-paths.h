#pragma once

#include "klee/Constraints.h"
#include "klee/ExprBuilder.h"

typedef uint32_t bits_t;

typedef struct {
  std::string symbol;
  bits_t offset;
  bits_t size;
} meta_t;

typedef struct {
  klee::ref<klee::Expr> expr;
  std::pair<bool, std::string> fn_ptr_name;
  klee::ref<klee::Expr> in;
  klee::ref<klee::Expr> out;
  std::vector<meta_t> meta;
} arg_t;

typedef struct call {
  std::string function_name;
  std::map<std::string, std::pair<klee::ref<klee::Expr>, klee::ref<klee::Expr>>>
      extra_vars;
  std::map<std::string, arg_t> args;
  klee::ref<klee::Expr> ret;
} call_t;

typedef std::vector<call_t> calls_t;

typedef struct call_path {
  std::string file_name;
  klee::ConstraintManager constraints;
  calls_t calls;
  std::map<std::string, const klee::Array *> arrays;
} call_path_t;

typedef std::pair<call_path_t *, calls_t> call_path_pair_t;

struct call_paths_t {
  std::vector<call_path_t *> cp;
  std::vector<calls_t> backup;

  call_paths_t() {}

  call_paths_t(const std::vector<call_path_t *> &_call_paths)
      : cp(_call_paths) {
    for (const auto &_cp : cp) {
      backup.emplace_back(_cp->calls);
    }

    assert(cp.size() == backup.size());

    merge_symbols();
  }

  size_t size() const { return cp.size(); }

  call_path_pair_t get(unsigned int i) {
    assert(i < size());
    return call_path_pair_t(cp[i], backup[i]);
  }

  void clear() {
    cp.clear();
    backup.clear();
  }

  void push_back(call_path_pair_t pair) {
    cp.push_back(pair.first);
    backup.push_back(pair.second);
  }

  void merge_symbols();

  static std::vector<std::string> skip_functions;
  static bool is_skip_function(const std::string &fname);
};

call_path_t *load_call_path(std::string file_name);
