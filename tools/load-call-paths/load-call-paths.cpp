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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include <dlfcn.h>
#include <expr/Parser.h>
#include <fstream>
#include <iostream>
#include <klee/Constraints.h>
#include <klee/Solver.h>
#include <vector>
#include <algorithm>
#include <iomanip>

// term colors
// src: https://stackoverflow.com/questions/9158150/colored-output-in-c/9158263
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#define DEBUG

#define UINT_16_SWAP_ENDIANNESS(p) ( (((p) & 0xff) << 8) | ((p) >> 8 & 0xff) )

namespace {
llvm::cl::list<std::string> InputCallPathFiles(llvm::cl::desc("<call paths>"),
                                               llvm::cl::Positional,
                                               llvm::cl::OneOrMore);
}

typedef struct {
  std::string function_name;
  std::map<std::string, std::pair<klee::ref<klee::Expr>,
                                  klee::ref<klee::Expr> > > extra_vars;
} call_t;

typedef struct {
  klee::ConstraintManager constraints;
  std::vector<call_t> calls;
  std::map<std::string, const klee::Array *> arrays;
} call_path_t;

call_path_t *load_call_path(std::string file_name,
                            std::vector<std::string> expressions_str,
                            std::deque<klee::ref<klee::Expr> > &expressions) {
  std::ifstream call_path_file(file_name);
  assert(call_path_file.is_open() && "Unable to open call path file.");

  call_path_t *call_path = new call_path_t;

  enum {
    STATE_INIT,
    STATE_KQUERY,
    STATE_CALLS,
    STATE_CALLS_MULTILINE,
    STATE_DONE
  } state = STATE_INIT;

  std::string kQuery;
  std::vector<klee::ref<klee::Expr> > exprs;
  std::set<std::string> declared_arrays;

  int parenthesis_level = 0;
  std::string current_extra_var;
  std::string current_extra_var_expr_str;
  std::vector<std::string> current_extra_var_exprs_str;

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

          for (auto eit : expressions_str) {
            kQuery += "\n         " + eit;
          }
          kQuery += "])";
        } else if (kQuery.substr(kQuery.length() - 6) == "false)") {
          kQuery = kQuery.substr(0, kQuery.length() - 1) + " [\n";

          for (auto eit : expressions_str) {
            kQuery += "\n         " + eit;
          }
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
          std::string array_name = line.substr(sizeof("array "));
          size_t delim = array_name.find("[");
          assert(delim != std::string::npos);
          array_name = array_name.substr(0, delim);
          declared_arrays.insert(array_name);
        }
      }
      break;

    case STATE_CALLS:
      if (line == ";;-- Constraints --") {
        for (size_t i = 0; i < expressions_str.size(); i++) {
          assert(!exprs.empty() && "Too few expressions in kQuery.");
          expressions.push_back(exprs.front());
          exprs.erase(exprs.begin());
        }

        assert(exprs.empty() && "Too many expressions in kQuery.");

        state = STATE_DONE;
      } else {
        size_t delim = line.find(":");
        assert(delim != std::string::npos);
        std::string preamble = line.substr(0, delim);
        line = line.substr(delim + 1);

        current_extra_var.clear();
        current_extra_var_exprs_str.clear();

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
          current_extra_var_expr_str += c;
          if (c == '(') {
            if (parenthesis_level == 0) {
              current_extra_var_expr_str = "(";
            }
            parenthesis_level++;
          } else if (c == ')') {
            parenthesis_level--;
            assert(parenthesis_level >= 0);

            if (parenthesis_level == 0) {
              current_extra_var_exprs_str.push_back(current_extra_var_expr_str);
            }
          }
        }

        if (parenthesis_level > 0) {
          state = STATE_CALLS_MULTILINE;
        } else {
          if (!current_extra_var.empty()) {
            assert(current_extra_var_exprs_str.size() == 2 &&
                   "Too many expression in extra variable.");
            if (current_extra_var_exprs_str[0] != "(...)") {
              assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
              call_path->calls.back().extra_vars[current_extra_var].first =
                  exprs[0];
              exprs.erase(exprs.begin(), exprs.begin() + 1);
            }
            if (current_extra_var_exprs_str[1] != "(...)") {
              assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
              call_path->calls.back().extra_vars[current_extra_var].second =
                  exprs[0];
              exprs.erase(exprs.begin(), exprs.begin() + 1);
            }
          }
        }
      }
    } break;

    case STATE_CALLS_MULTILINE: {
      current_extra_var_expr_str += " ";
      for (char c : line) {
        current_extra_var_expr_str += c;
        if (c == '(') {
          if (parenthesis_level == 0) {
            current_extra_var_expr_str = "(";
          }
          parenthesis_level++;
        } else if (c == ')') {
          parenthesis_level--;
          assert(parenthesis_level >= 0);

          if (parenthesis_level == 0) {
            current_extra_var_exprs_str.push_back(current_extra_var_expr_str);
          }
        }
      }

      if (parenthesis_level == 0) {
        if (!current_extra_var.empty()) {
          assert(current_extra_var_exprs_str.size() == 2 &&
                 "Too many expression in extra variable.");
          if (current_extra_var_exprs_str[0] != "(...)") {
            assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
            call_path->calls.back().extra_vars[current_extra_var].first =
                exprs[0];
            exprs.erase(exprs.begin(), exprs.begin() + 1);
          }
          if (current_extra_var_exprs_str[1] != "(...)") {
            assert(exprs.size() >= 1 && "Not enough expression in kQuery.");
            call_path->calls.back().extra_vars[current_extra_var].second =
                exprs[0];
            exprs.erase(exprs.begin(), exprs.begin() + 1);
          }
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

uint64_t evaluate_expr(klee::expr::ExprHandle expr, klee::Expr::Width width, klee::ConstraintManager constraints)
{
  klee::Solver *solver = klee::createCoreSolver(klee::Z3_SOLVER);
  assert(solver);

  solver = createCexCachingSolver(solver);
  solver = createCachingSolver(solver);
  solver = createIndependentSolver(solver);

  klee::Query sat_query(constraints, expr);
  klee::ref<klee::ConstantExpr> result;

  assert(solver->getValue(sat_query, result));

  return result.get()->getZExtValue(width);
}

std::vector<unsigned> readLSB_byte_indexes(klee::ReadExpr *expr, klee::ConstraintManager constraints)
{
  std::vector<unsigned> bytes;
  uint64_t index = evaluate_expr(expr->index, expr->index->getWidth(), constraints);
  bytes.push_back(index);
  return bytes;
}

std::vector<unsigned> readLSB_byte_indexes(klee::ConcatExpr *expr, klee::ConstraintManager constraints)
{
  std::vector<unsigned> bytes;
  std::vector<unsigned> right_bytes, left_bytes;

  if (expr->getRight()->getKind() == klee::Expr::Kind::Concat)
  {
    klee::ConcatExpr *right = cast<klee::ConcatExpr>(expr->getRight());
    right_bytes = readLSB_byte_indexes(right, constraints);
  } else if (expr->getRight()->getKind() == klee::Expr::Read)
  {
    klee::ReadExpr *right = cast<klee::ReadExpr>(expr->getRight());
    right_bytes = readLSB_byte_indexes(right, constraints);
  } else {
    assert(false && "Unknown expression on readLSB_byte_indexes");
  }

  bytes.insert(bytes.end(), right_bytes.begin(), right_bytes.end());

  if (expr->getLeft()->getKind() == klee::Expr::Kind::Concat)
  {
    klee::ConcatExpr *left = cast<klee::ConcatExpr>(expr->getLeft());
    left_bytes = readLSB_byte_indexes(left, constraints);
  } else if (expr->getLeft()->getKind() == klee::Expr::Read)
  {
    klee::ReadExpr *left = cast<klee::ReadExpr>(expr->getLeft());
    left_bytes = readLSB_byte_indexes(left, constraints);
  } else {
    assert(false && "Unknown expression on readLSB_byte_indexes");
  }

  bytes.insert(bytes.end(), left_bytes.begin(), left_bytes.end());

  return bytes;
}

void readLSB_parse(
  klee::expr::ExprHandle expr,
  klee::ConstraintManager constraints,
  unsigned &offset,
  unsigned &size // bits
  )
{
  std::vector<unsigned> bytes_read;

  switch (expr->getKind())
  {
    case klee::Expr::Kind::Read: 
    {
      klee::ReadExpr *read = cast<klee::ReadExpr>(expr);
      bytes_read = readLSB_byte_indexes(read, constraints);
      size = read->getWidth();
      break;
    }

    case klee::Expr::Kind::Concat:
    {
      klee::ConcatExpr *concat = cast<klee::ConcatExpr>(expr);
      bytes_read = readLSB_byte_indexes(concat, constraints);
      size = concat->getWidth();
      break;
    }

    default:
      std::cout << "cast missing" << std::endl;
  }

  
  offset = *std::min_element(bytes_read.begin(), bytes_read.end());
}

bool has_packet(klee::expr::ExprHandle expr, klee::ConstraintManager constraints, std::vector<unsigned> &bytes_read)
{
  switch (expr->getKind())
  {
    case klee::Expr::Kind::Concat:
    {
      klee::ConcatExpr *concat = cast<klee::ConcatExpr>(expr);
      return has_packet(concat->getLeft(), constraints, bytes_read) &&
        has_packet(concat->getRight(), constraints, bytes_read);
    }

    case klee::Expr::Kind::Read:
    {
      klee::ReadExpr *read = cast<klee::ReadExpr>(expr);
      
      uint64_t index = evaluate_expr(read->index, read->index->getWidth(), constraints);
      bytes_read.push_back(index);

      if (read->updates.root == nullptr) return false;
      if (read->updates.root->getName() != "packet_chunks") return false;
      
      return true;
    }

    default:
      for (unsigned i = 0; i < expr->getNumKids(); i++)
        if (has_packet(expr->getKid(i), constraints, bytes_read)) return true;
      return false;
  }
}

struct chunk_state {

  struct proto_data {
    unsigned code;
    bool complete;

    proto_data() {}

    proto_data(unsigned _code, bool _complete) {
      code = _code;
      complete = _complete;
    }
  };

  klee::expr::ExprHandle expr;
  std::vector<klee::expr::ExprHandle> exprs_appended;

  unsigned offset;
  unsigned borrowed;
  unsigned layer;

  std::pair<chunk_state::proto_data, bool> proto;
  std::vector<unsigned> packet_fields_deps;

  chunk_state() {
    proto.second = false;
  }

  chunk_state(klee::expr::ExprHandle _expr) {
    expr = _expr;
    proto.second = false;
  }

  void add_proto(unsigned _code, bool _complete) {
    std::pair<chunk_state::proto_data, bool> new_proto
      (chunk_state::proto_data(_code, _complete), true);
    proto.swap(new_proto);
  }

  bool is_complete() {
    return !proto.second || proto.first.complete;
  }

  void append(chunk_state chunk) {
    if (!proto.second)
      assert(false && "proto not set");

    exprs_appended.push_back(chunk.expr);
    proto.first.complete = true;
  }
};

std::string expr_to_string(klee::expr::ExprHandle expr) {
  std::string expr_str;
  llvm::raw_string_ostream os(expr_str);
  expr->print(os);
  os.str();
  return expr_str;
}

struct mem_access {
  klee::expr::ExprHandle expr;
  std::string interface;
  std::pair<chunk_state, bool> chunk;


  mem_access(std::string _interface, klee::expr::ExprHandle _expr) {
    chunk.second = false;
    interface = _interface;
    expr = _expr;
  }

  void add_chunk(chunk_state _chunk)
  {
    std::pair<chunk_state, bool> new_chunk(_chunk, true);
    chunk.swap(new_chunk);
  }

  void append_dep(unsigned dep)
  {
    if (!chunk.second) {
      std::cout
        << RED
        << "[ERROR] no chunk stored, can't add dependency."
        << RESET
        << std::endl;
      exit(1);
    }

    chunk.first.packet_fields_deps.push_back(dep - chunk.first.offset);
  }

  void print()
  {
    std::cout << "interface: " << interface << std::endl;
    
    std::cout << "expr:        " << std::endl;
    std::cout << expr_to_string(expr) << std::endl;

    if (!chunk.second) return;

    std::cout << "chunk:       " << std::endl;
    std::cout << expr_to_string(chunk.first.expr) << std::endl;

    for (auto appended : chunk.first.exprs_appended) {
      std::cout << "appended:    " << std::endl;
      std::cout << expr_to_string(appended) << std::endl;
    }

    std::cout << "layer:       " << chunk.first.layer << std::endl;
    std::cout << "offset:      " << chunk.first.offset << std::endl;
    std::cout << "borrowed:    " << chunk.first.borrowed << std::endl;

    if (chunk.first.proto.second) {
      std::cout << "proto:       0x"
        << std::setfill('0')
        << std::setw(4)
        << std::hex
        << chunk.first.proto.first.code
        << std::dec
        << std::endl;
      
      std::cout << "dependencies:" << std::endl;
      for (unsigned dep : chunk.first.packet_fields_deps)
        std::cout << "          byte " << dep << std::endl;
    }

  }

};

void proto_from_chunk(
  chunk_state             prev_chunk,
  klee::ConstraintManager constraints,
  chunk_state             &chunk
)
{
  unsigned proto;

  klee::ExprBuilder *exprBuilder = klee::createDefaultExprBuilder();;
  klee::Solver *solver = klee::createCoreSolver(klee::Z3_SOLVER);

  solver = createCexCachingSolver(solver);
  solver = createCachingSolver(solver);
  solver = createIndependentSolver(solver);

  if (chunk.layer == 3) {
    klee::ref<klee::Expr> proto_expr =
      exprBuilder->Extract(prev_chunk.expr, 12 * 8, klee::Expr::Int16);

    klee::Query proto_query(constraints, proto_expr);
    klee::ref<klee::ConstantExpr> proto_value;

    assert(solver->getValue(proto_query, proto_value));

    proto = proto_value.get()->getZExtValue(klee::Expr::Int16);
    proto = UINT_16_SWAP_ENDIANNESS(proto);

    if (proto == 0x0800) // IP
    {
      klee::ref<klee::Expr> version_ihl_expr =
        exprBuilder->Extract(chunk.expr, 0, klee::Expr::Int8);

      klee::Query version_ihl_query(constraints, version_ihl_expr);
      klee::ref<klee::ConstantExpr> version_ihl_value;

      assert(solver->getValue(version_ihl_query, version_ihl_value));

      unsigned version_ihl = 0;
      version_ihl =
        version_ihl_value.get()->getZExtValue(klee::Expr::Int8);

      unsigned ihl = version_ihl & 0xf;

      chunk.add_proto(proto, ihl <= 5);
    } else {
      std::cout
      << MAGENTA
      << "[WARNING] Layer 3 protocol not in set { IP, VLAN }"
      << RESET
      << std::endl;
    }
  } else if (chunk.layer == 4) {
    klee::ref<klee::Expr> proto_expr =
      exprBuilder->Extract(prev_chunk.expr, 9 * 8, klee::Expr::Int8);

    klee::Query proto_query(constraints, proto_expr);
    klee::ref<klee::ConstantExpr> proto_value;

    assert(solver->getValue(proto_query, proto_value));

    proto = proto_value.get()->getZExtValue(klee::Expr::Int8);

    chunk.add_proto(proto, true);
  } else {
    std::cout
      << RED
      << "[WARNING] Not implemented: trying to parse layer "
      << chunk.layer
      << RESET
      << std::endl;
    
    return;
  }
}

void store_chunk(
  klee::expr::ExprHandle           chunk_expr,
  klee::ConstraintManager          constraints,
  std::vector<chunk_state>         &chunks
)
{
  chunk_state chunk(chunk_expr);

  readLSB_parse(chunk.expr, constraints, chunk.offset, chunk.borrowed);

  if (chunks.size() == 0 || chunks.back().is_complete()) {
    if (chunks.size() == 0)
      chunk.layer = 2;
    else {
      chunk.layer = chunks.back().layer + 1;
      proto_from_chunk(chunks.back(), constraints, chunk);
    }

    chunks.push_back(chunk);
  } else if (chunks.size()) {
    chunks.back().append(chunk);
  }

}

void mem_access_process(
  std::string             interface,
  klee::expr::ExprHandle  expr,
  klee::ConstraintManager constraints,
  chunk_state             current_chunk,
  std::vector<mem_access> &mem_accesses
)
{
  std::vector<unsigned> bytes_read;
  
  mem_access ma(interface, expr);
  mem_accesses.push_back(ma);

  if (!has_packet(expr, constraints, bytes_read))
    return;

  mem_access &last_ma = mem_accesses.back();
  last_ma.add_chunk(current_chunk);

  for (auto byte_read : bytes_read)
    last_ma.append_dep(byte_read);
}

std::vector<mem_access> parse_call_path(call_path_t *call_path)
{
  std::vector<mem_access> mem_accesses;
  std::vector<chunk_state> chunks;

  for (auto call : call_path->calls) {
    std::cout << "[CALL] " << call.function_name << std::endl;
    
    if (call.function_name == "packet_borrow_next_chunk") {
      std::cout << "  grabbing chunk info" << std::endl;

      assert(call.extra_vars.count("the_chunk"));
      assert(!call.extra_vars["the_chunk"].second.isNull());

      store_chunk(
        call.extra_vars["the_chunk"].second,
        call_path->constraints,
        chunks
      );
    }

    else if (call.extra_vars.count("the_key")) {
      std::cout << "  grabbing mem access info" << std::endl;
      
      assert(!call.extra_vars["the_key"].first.isNull());

      mem_access_process(
        call.function_name,
        call.extra_vars["the_key"].first,
        call_path->constraints,
        chunks.back(),
        mem_accesses);
    }
  }

  return mem_accesses;
}

int main(int argc, char **argv, char **envp) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  std::vector<call_path_t *> call_paths;
  std::vector< std::pair<std::string, mem_access> > mem_accesses;

  for (auto file : InputCallPathFiles) {
    std::cout << "Loading: " << file << std::endl;

    std::vector<std::string> expressions_str;
    std::deque<klee::ref<klee::Expr> > expressions;
    
    call_path_t *call_path = load_call_path(file, expressions_str, expressions);
    std::vector<mem_access> mas = parse_call_path(call_path);

    for (auto ma : mas)
    {
      std::pair<std::string, mem_access> file_ma(file, ma);
      mem_accesses.push_back(file_ma);
    }
  }

  for (auto ma : mem_accesses)
  {
    std::cout << "\n=========== MEMORY ACCESS ===========" << std::endl;
    std::cout << "file: " << ma.first << std::endl;
    ma.second.print();
  }

  return 0;
}
