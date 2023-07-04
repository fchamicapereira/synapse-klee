/* -*- mode: c++; c-basic-offset: 2; -*- */

//===-- ktest-dehavoc.cpp ---------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/ExprBuilder.h"
#include "klee/perf-contracts.h"
#include "llvm/Support/MemoryBuffer.h"
#include <klee/Constraints.h>
#include <klee/Solver.h>

#include <dlfcn.h>
#include <expr/Parser.h>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../klee-util/klee-util.h"
#include "load-call-paths.h"

#define DEBUG

std::vector<std::string> call_paths_t::skip_functions{
    "loop_invariant_consume",
    "loop_invariant_produce",
    "packet_receive",
    "packet_state_total_length",
    "packet_free",
    "packet_send",
    "packet_get_unread_length"};

bool call_paths_t::is_skip_function(const std::string &fname) {
  auto found_it = std::find(call_paths_t::skip_functions.begin(),
                            call_paths_t::skip_functions.end(), fname);
  return found_it != call_paths_t::skip_functions.end();
}

klee::ref<klee::Expr> parse_expr(const std::set<std::string> &declared_arrays,
                                 const std::string &expr_str) {
  std::stringstream kQuery_builder;

  for (auto arr : declared_arrays) {
    kQuery_builder << arr;
    kQuery_builder << "\n";
  }

  kQuery_builder << "(query [] false [";
  kQuery_builder << "(" << expr_str << ")";
  kQuery_builder << "])";

  auto kQuery = kQuery_builder.str();

  auto MB = llvm::MemoryBuffer::getMemBuffer(kQuery);
  auto Builder = klee::createDefaultExprBuilder();
  auto P = klee::expr::Parser::Create("", MB, Builder, true);

  while (auto D = P->ParseTopLevelDecl()) {
    assert(!P->GetNumErrors() && "Error parsing kquery in call path file.");

    if (auto QC = dyn_cast<klee::expr::QueryCommand>(D)) {
      assert(QC->Values.size() == 1);
      return QC->Values[0];
    }
  }

  assert(false && "Error parsing expr");
  std::cerr << "Error parsing expr: " << expr_str << "\n";
  exit(1);
}

call_path_t *load_call_path(std::string file_name) {
  std::ifstream call_path_file(file_name);
  assert(call_path_file.is_open() && "Unable to open call path file.");

  call_path_t *call_path = new call_path_t;
  call_path->file_name = file_name;

  enum {
    STATE_INIT,
    STATE_KQUERY,
    STATE_CALLS,
    STATE_CALLS_MULTILINE,
    STATE_DONE
  } state = STATE_INIT;

  std::string kQuery;
  std::vector<klee::ref<klee::Expr>> exprs;
  std::set<std::string> declared_arrays;

  int parenthesis_level = 0;

  std::string current_extra_var;
  std::string current_arg;
  std::string current_arg_name;

  std::string current_expr_str;
  std::vector<std::string> current_exprs_str;

  while (!call_path_file.eof()) {
    std::string line;
    std::getline(call_path_file, line);

    switch (state) {
    case STATE_INIT: {
      if (line == ";;-- kQuery --") {
        state = STATE_KQUERY;
      }
    } break;

    case STATE_KQUERY: {
      if (line == ";;-- Calls --") {
        if (kQuery.substr(kQuery.length() - 2) == "])") {
          kQuery = kQuery.substr(0, kQuery.length() - 2) + "\n";
          kQuery += "])";
        } else if (kQuery.substr(kQuery.length() - 6) == "false)") {
          kQuery = kQuery.substr(0, kQuery.length() - 1) + " [\n";
          kQuery += "])";
        }

        llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer(kQuery);
        klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
        klee::expr::Parser *P =
            klee::expr::Parser::Create("", MB, Builder, false);
        while (klee::expr::Decl *D = P->ParseTopLevelDecl()) {
          assert(!P->GetNumErrors() &&
                 "Error parsing kquery in call path file.");
          if (klee::expr::ArrayDecl *AD = dyn_cast<klee::expr::ArrayDecl>(D)) {
            call_path->arrays[AD->Root->name] = AD->Root;
          } else if (klee::expr::QueryCommand *QC =
                         dyn_cast<klee::expr::QueryCommand>(D)) {
            call_path->constraints = klee::ConstraintManager(QC->Constraints);
            exprs = QC->Values;
            break;
          }
        }

        state = STATE_CALLS;
      } else {
        kQuery += "\n" + line;

        if (line.substr(0, sizeof("array ") - 1) == "array ") {
          declared_arrays.insert(line);
        }
      }
      break;

    case STATE_CALLS:
      if (line == ";;-- Constraints --") {
        assert(exprs.empty() && "Too many expressions in kQuery.");
        state = STATE_DONE;
      } else {
        size_t delim = line.find(":");
        assert(delim != std::string::npos);
        std::string preamble = line.substr(0, delim);
        line = line.substr(delim + 1);

        current_extra_var.clear();
        current_exprs_str.clear();

        if (preamble == "extra") {
          while (line[0] == ' ') {
            line = line.substr(1);
          }

          delim = line.find("&");
          assert(delim != std::string::npos);
          current_extra_var = line.substr(0, delim);
          line = line.substr(delim + 1);

          delim = line.find("[");
          assert(delim != std::string::npos);
          line = line.substr(delim + 1);
        } else {
          call_path->calls.emplace_back();

          delim = line.find("(");
          assert(delim != std::string::npos);
          call_path->calls.back().function_name = line.substr(0, delim);
        }

        for (char c : line) {
          current_expr_str += c;
          if (c == '(') {
            if (parenthesis_level == 0) {
              current_expr_str = "(";
            }
            parenthesis_level++;
          } else if (c == ')') {
            parenthesis_level--;
            assert(parenthesis_level >= 0);

            if (parenthesis_level == 0) {
              current_exprs_str.push_back(current_expr_str);
            }
          }
        }

        if (current_exprs_str.size() && parenthesis_level == 0) {
          auto last_store = current_exprs_str.back();
          if (line.size() < last_store.size())
            last_store = last_store.substr(last_store.size() - line.size());
          auto remainder_delim = line.find(last_store);
          auto remainder = line.substr(remainder_delim + last_store.size());
          auto ret_symbol = std::string("-> ");
          auto ret_delim = remainder.find(ret_symbol);
          if (ret_delim != std::string::npos &&
              remainder.substr(ret_symbol.size() + 1) != "[]") {
            auto ret = remainder.substr(ret_symbol.size() + 1);
            current_exprs_str.push_back(ret);
          }
        }

        if (parenthesis_level > 0) {
          state = STATE_CALLS_MULTILINE;
        } else {
          if (!current_extra_var.empty()) {
            assert(current_exprs_str.size() == 2 &&
                   "Too many expression in extra variable.");
            if (current_exprs_str[0] != "(...)") {
              assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
              call_path->calls.back().extra_vars[current_extra_var].first =
                  exprs[0];
              exprs.erase(exprs.begin());
              current_exprs_str.erase(current_exprs_str.begin(),
                                      current_exprs_str.begin() + 2);
            }
            if (current_exprs_str[1] != "(...)") {
              assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
              call_path->calls.back().extra_vars[current_extra_var].second =
                  exprs[0];
              exprs.erase(exprs.begin());
              current_exprs_str.erase(current_exprs_str.begin(),
                                      current_exprs_str.begin() + 2);
            }
          } else {
            bool parsed_last_arg = false;
            while (!parsed_last_arg) {
              if (current_exprs_str[0] == "()")
                break;
              delim = current_exprs_str[0].find(",");
              if (delim == std::string::npos) {
                delim = current_exprs_str[0].size() - 1;
                parsed_last_arg = true;
              }
              current_arg = current_exprs_str[0].substr(0, delim);
              if (current_arg[0] == '(')
                current_arg = current_arg.substr(1);
              current_exprs_str[0] = current_exprs_str[0].substr(delim + 1);
              delim = current_arg.find(":");
              assert(delim != std::string::npos);
              current_arg_name = current_arg.substr(0, delim);
              current_arg = current_arg.substr(delim + 1);

              delim = current_arg.find("&");
              if (delim == std::string::npos) {
                assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
                call_path->calls.back().args[current_arg_name].expr = exprs[0];
                exprs.erase(exprs.begin(), exprs.begin() + 1);
              } else {
                call_path->calls.back().args[current_arg_name].expr = exprs[0];
                exprs.erase(exprs.begin(), exprs.begin() + 1);

                if (current_arg.substr(delim + 1) == "[...]") {
                  continue;
                }

                if (current_arg.substr(delim + 1)[0] != '[') {
                  call_path->calls.back().args[current_arg_name].fn_ptr_name =
                      std::make_pair(true, current_arg.substr(delim + 1));
                  continue;
                }

                current_arg = current_arg.substr(delim + 2);

                delim = current_arg.find("]");
                assert(delim != std::string::npos);

                auto current_arg_meta = current_arg.substr(delim + 1);
                current_arg = current_arg.substr(0, delim);

                delim = current_arg.find("->");
                assert(delim != std::string::npos);

                if (current_arg.substr(0, delim).size()) {
                  assert(exprs.size() >= 1 &&
                         "Not enough expression in kQuery.");
                  call_path->calls.back().args[current_arg_name].in = exprs[0];
                  exprs.erase(exprs.begin(), exprs.begin() + 1);
                }

                if (current_arg.substr(delim + 2).size()) {
                  assert(exprs.size() >= 1 &&
                         "Not enough expression in kQuery.");
                  call_path->calls.back().args[current_arg_name].out = exprs[0];
                  exprs.erase(exprs.begin(), exprs.begin() + 1);
                }

                if (current_arg_meta.size()) {
                  std::vector<std::string> expr_parts;

                  while (current_arg_meta.size()) {
                    auto start_delim = current_arg_meta.find("[");
                    auto end_delim = current_arg_meta.find("]");

                    assert(start_delim != std::string::npos);
                    assert(end_delim != std::string::npos);

                    auto size = end_delim - start_delim - 1;
                    assert(size > 0);

                    auto part =
                        current_arg_meta.substr(start_delim + 1, end_delim - 1);
                    current_arg_meta = current_arg_meta.substr(end_delim + 1);

                    expr_parts.push_back(part);
                  }

                  bits_t offset = 0;

                  for (auto part : expr_parts) {
                    delim = part.find("->");
                    assert(delim != std::string::npos);

                    part = part.substr(0, delim);

                    auto open_delim = part.find("(");
                    auto close_delim = part.find(")");

                    assert(open_delim != std::string::npos);
                    assert(close_delim != std::string::npos);

                    auto symbol = part.substr(0, open_delim);
                    auto meta_expr_str = part.substr(open_delim + 1);
                    meta_expr_str =
                        meta_expr_str.substr(0, meta_expr_str.size() - 1);

                    auto meta_expr = parse_expr(declared_arrays, meta_expr_str);
                    auto meta_size = meta_expr->getWidth();
                    auto meta = meta_t{symbol, offset, meta_size};

                    offset += meta_size;

                    call_path->calls.back()
                        .args[current_arg_name]
                        .meta.push_back(meta);
                  }
                }
              }
            }
            current_exprs_str.erase(current_exprs_str.begin());
          }

          if (current_exprs_str.size()) {
            call_path->calls.back().ret = exprs[0];
            exprs.erase(exprs.begin());
          }
        }
      }
    } break;

    case STATE_CALLS_MULTILINE: {
      current_expr_str += " ";
      for (char c : line) {
        current_expr_str += c;
        if (c == '(') {
          if (parenthesis_level == 0) {
            current_expr_str = "(";
          }
          parenthesis_level++;
        } else if (c == ')') {
          parenthesis_level--;
          assert(parenthesis_level >= 0);

          if (parenthesis_level == 0) {
            current_exprs_str.push_back(current_expr_str);
          }
        }
      }

      if (current_exprs_str.size() && parenthesis_level == 0) {
        auto last_store = current_exprs_str.back();
        if (line.size() < last_store.size())
          last_store = last_store.substr(last_store.size() - line.size());
        auto remainder_delim = line.find(last_store);
        auto remainder = line.substr(remainder_delim + last_store.size());
        auto ret_symbol = std::string("-> ");
        auto ret_delim = remainder.find(ret_symbol);
        if (ret_delim != std::string::npos &&
            remainder.substr(ret_symbol.size() + 1) != "[]") {
          auto ret = remainder.substr(ret_symbol.size() + 1);
          current_exprs_str.push_back(ret);
        }
      }

      if (parenthesis_level == 0) {
        if (!current_extra_var.empty()) {
          assert(current_exprs_str.size() == 2 &&
                 "Too many expression in extra variable.");
          if (current_exprs_str[0] != "(...)") {
            assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
            call_path->calls.back().extra_vars[current_extra_var].first =
                exprs[0];
            exprs.erase(exprs.begin());
            current_exprs_str.erase(current_exprs_str.begin(),
                                    current_exprs_str.begin() + 2);
          }
          if (current_exprs_str[1] != "(...)") {
            assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
            call_path->calls.back().extra_vars[current_extra_var].second =
                exprs[0];
            exprs.erase(exprs.begin());
            current_exprs_str.erase(current_exprs_str.begin(),
                                    current_exprs_str.begin() + 2);
          }
        } else {
          bool parsed_last_arg = false;
          size_t delim;

          while (!parsed_last_arg) {
            if (current_exprs_str[0] == "()")
              break;
            delim = current_exprs_str[0].find(",");
            if (delim == std::string::npos) {
              delim = current_exprs_str[0].size() - 1;
              parsed_last_arg = true;
            }
            current_arg = current_exprs_str[0].substr(0, delim);
            if (current_arg[0] == '(')
              current_arg = current_arg.substr(1);
            current_exprs_str[0] = current_exprs_str[0].substr(delim + 1);
            delim = current_arg.find(":");
            assert(delim != std::string::npos);
            current_arg_name = current_arg.substr(0, delim);
            current_arg = current_arg.substr(delim + 1);

            delim = current_arg.find("&");
            if (delim == std::string::npos) {
              assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
              call_path->calls.back().args[current_arg_name].expr = exprs[0];
              exprs.erase(exprs.begin(), exprs.begin() + 1);
            } else {
              call_path->calls.back().args[current_arg_name].expr = exprs[0];
              exprs.erase(exprs.begin(), exprs.begin() + 1);

              if (current_arg.substr(delim + 1) == "[...]") {
                continue;
              }

              if (current_arg.substr(delim + 1)[0] != '[') {
                call_path->calls.back().args[current_arg_name].fn_ptr_name =
                    std::make_pair(true, current_arg.substr(delim + 1));
                continue;
              }

              current_arg = current_arg.substr(delim + 2);

              delim = current_arg.find("]");
              assert(delim != std::string::npos);

              auto current_arg_meta = current_arg.substr(delim + 1);
              current_arg = current_arg.substr(0, delim);

              delim = current_arg.find("->");
              assert(delim != std::string::npos);

              if (current_arg.substr(0, delim).size()) {
                assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
                call_path->calls.back().args[current_arg_name].in = exprs[0];
                exprs.erase(exprs.begin(), exprs.begin() + 1);
              }

              if (current_arg.substr(delim + 2).size()) {
                assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
                call_path->calls.back().args[current_arg_name].out = exprs[0];
                exprs.erase(exprs.begin(), exprs.begin() + 1);
              }

              if (current_arg_meta.size()) {
                std::vector<std::string> expr_parts;

                while (current_arg_meta.size()) {
                  auto start_delim = current_arg_meta.find("[");
                  auto end_delim = current_arg_meta.find("]");

                  assert(start_delim != std::string::npos);
                  assert(end_delim != std::string::npos);

                  auto size = end_delim - start_delim - 1;
                  assert(size > 0);

                  auto part =
                      current_arg_meta.substr(start_delim + 1, end_delim - 1);
                  current_arg_meta = current_arg_meta.substr(end_delim + 1);

                  expr_parts.push_back(part);
                }

                bits_t offset = 0;

                for (auto part : expr_parts) {
                  delim = part.find("->");
                  assert(delim != std::string::npos);

                  part = part.substr(0, delim);

                  auto open_delim = part.find("(");
                  auto close_delim = part.find(")");

                  assert(open_delim != std::string::npos);
                  assert(close_delim != std::string::npos);

                  auto symbol = part.substr(0, open_delim);
                  auto meta_expr_str = part.substr(open_delim + 1);
                  meta_expr_str =
                      meta_expr_str.substr(0, meta_expr_str.size() - 1);

                  auto meta_expr = parse_expr(declared_arrays, meta_expr_str);
                  auto meta_size = meta_expr->getWidth();
                  auto meta = meta_t{symbol, offset, meta_size};

                  offset += meta_size;

                  call_path->calls.back().args[current_arg_name].meta.push_back(
                      meta);
                }
              }
            }
          }

          current_exprs_str.erase(current_exprs_str.begin());
        }

        if (current_exprs_str.size()) {
          call_path->calls.back().ret = exprs[0];
          exprs.erase(exprs.begin());
        }

        state = STATE_CALLS;
      }

      continue;
    } break;

    case STATE_DONE: {
      continue;
    } break;

    default: {
      assert(false && "Invalid call path file.");
    } break;
    }
  }

  return call_path;
}

struct symbols_merger_t {
  std::unordered_map<std::string, klee::UpdateList> roots_updates;
  kutil::ReplaceSymbols replacer;

  klee::ConstraintManager
  save_and_merge(const klee::ConstraintManager &constraints) {
    klee::ConstraintManager new_constraints;

    for (auto c : constraints) {
      auto new_c = save_and_merge(c);
      new_constraints.addConstraint(new_c);
    }

    return new_constraints;
  }

  call_t save_and_merge(const call_t &call) {
    call_t new_call = call;

    for (auto it = call.args.begin(); it != call.args.end(); it++) {
      const auto &arg = call.args.at(it->first);

      new_call.args[it->first].expr = save_and_merge(arg.expr);
      new_call.args[it->first].in = save_and_merge(arg.in);
      new_call.args[it->first].out = save_and_merge(arg.out);
    }

    for (auto it = call.extra_vars.begin(); it != call.extra_vars.end(); it++) {
      const auto &extra_var = call.extra_vars.at(it->first);

      new_call.extra_vars[it->first].first = save_and_merge(extra_var.first);
      new_call.extra_vars[it->first].second = save_and_merge(extra_var.second);
    }

    new_call.ret = save_and_merge(call.ret);

    return new_call;
  }

  klee::ref<klee::Expr> save_and_merge(klee::ref<klee::Expr> expr) {
    if (expr.isNull()) {
      return expr;
    }

    kutil::RetrieveSymbols retriever;
    retriever.visit(expr);

    auto expr_roots_updates = retriever.get_retrieved_roots_updates();

    for (auto it = expr_roots_updates.begin(); it != expr_roots_updates.end();
         it++) {
      if (roots_updates.find(it->first) != roots_updates.end()) {
        continue;
      }

      roots_updates.insert({it->first, it->second});
    }

    replacer.add_roots_updates(expr_roots_updates);
    return replacer.visit(expr);
  }
};

void call_paths_t::merge_symbols() {
  symbols_merger_t merger;

  for (auto call_path : cp) {
    auto constraints = call_path->constraints;
    auto new_constraints = merger.save_and_merge(constraints);
    call_path->constraints = new_constraints;

    for (auto i = 0u; i < call_path->calls.size(); i++) {
      const auto &call = call_path->calls[i];
      auto new_call = merger.save_and_merge(call);
      call_path->calls[i] = new_call;
    }
  }
}
