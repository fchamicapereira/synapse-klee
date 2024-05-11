/* -*- mode: c++; c-basic-offset: 2; -*- */

//===-- ktest-dehavoc.cpp ---------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <vector>

#include "load-call-paths.h"

#include "llvm/Support/CommandLine.h"

namespace {
llvm::cl::list<std::string> InputCallPathFiles(llvm::cl::desc("<call paths>"),
                                               llvm::cl::Positional,
                                               llvm::cl::OneOrMore);
}

#define DEBUG

int main(int argc, char **argv, char **envp) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  call_paths_t call_paths;

  for (auto file : InputCallPathFiles) {
    std::cerr << "Loading: " << file << std::endl;
    call_paths.cps.push_back(load_call_path(file));
  }

  for (unsigned i = 0; i < call_paths.cps.size(); i++) {
    std::cerr << "Call Path " << i << std::endl;
    std::cerr << "  Assuming: ";
    for (auto constraint : call_paths.cps[i]->constraints) {
      constraint->dump();
      std::cerr << std::endl;
    }
    std::cerr << "  Calls:" << std::endl;
    for (auto call : call_paths.cps[i]->calls) {
      std::cerr << "    Function: " << call.function_name << std::endl;
      if (!call.args.empty()) {
        std::cerr << "      With Args:" << std::endl;
        for (auto arg : call.args) {
          std::cerr << "        " << arg.first << std::endl;

          std::cerr << "            Expr: ";
          arg.second.expr->dump();

          if (!arg.second.in.isNull()) {
            std::cerr << "            Before: ";
            arg.second.in->dump();
          }

          if (!arg.second.out.isNull()) {
            std::cerr << "            After: ";
            arg.second.out->dump();
          }

          if (arg.second.meta.size()) {
            std::cerr << "            Meta: \n";
            for (auto meta : arg.second.meta) {
              std::cerr << "                  " << meta.symbol;
              std::cerr << " (" << meta.size << " bits)\n";
            }

            {
              char c;
              std::cin >> c;
            }
          }

          if (arg.second.fn_ptr_name.first) {
            std::cerr << "            Fn: " << arg.second.fn_ptr_name.second;
            std::cerr << std::endl;
          }
        }
      }
      if (!call.extra_vars.empty()) {
        std::cerr << "      With Extra Vars:" << std::endl;
        for (auto extra_var : call.extra_vars) {
          std::cerr << "        " << extra_var.first << std::endl;
          if (!extra_var.second.first.isNull()) {
            std::cerr << "            Before: ";
            extra_var.second.first->dump();
          }
          if (!extra_var.second.second.isNull()) {
            std::cerr << "            After: ";
            extra_var.second.second->dump();
          }
        }
      }

      if (!call.ret.isNull()) {
        std::cerr << "      With Ret: ";
        call.ret->dump();
      }
    }
  }

  call_paths.free_call_paths();

  return 0;
}
