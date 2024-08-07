#include "klee/ExprBuilder.h"
#include "klee/perf-contracts.h"
#include "klee/util/ArrayCache.h"
#include "klee/util/ExprSMTLIBPrinter.h"
#include "klee/util/ExprVisitor.h"
#include "llvm/Support/CommandLine.h"
#include <klee/Constraints.h>
#include <klee/Solver.h>

#include <algorithm>
#include <dlfcn.h>
#include <expr/Parser.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <stack>
#include <utility>
#include <vector>

#include "ast.h"
#include "call-paths-to-bdd.h"
#include "load-call-paths.h"
#include "nodes.h"

namespace {
llvm::cl::OptionCategory SynthesizerCat("Synthesizer specific options");

llvm::cl::opt<std::string>
    InputBDDFile("in", llvm::cl::desc("Input file for BDD deserialization."),
                 llvm::cl::Required, llvm::cl::cat(SynthesizerCat));

llvm::cl::opt<std::string>
    Out("out",
        llvm::cl::desc("Output C++ file of the syntethized code. If omited, "
                       "code will be dumped to stdout."),
        llvm::cl::cat(SynthesizerCat));

llvm::cl::opt<std::string>
    XML("xml",
        llvm::cl::desc("Output file of the syntethized code's XML. If omited, "
                       "XML will not be dumped."),
        llvm::cl::cat(SynthesizerCat));

llvm::cl::opt<TargetOption>
    Target("target", llvm::cl::desc("Output file's target."),
           llvm::cl::cat(SynthesizerCat),
           llvm::cl::values(clEnumValN(SEQUENTIAL, "seq", "Sequential"),
                            clEnumValN(BDD_PATH_PROFILER, "bdd-profiler",
                                       "BDD path profiler"),
                            clEnumValEnd),
           llvm::cl::Required);
} // namespace

Node_ptr update_path_profiler_rate(AST &ast, uint64_t node_id) {
  Expr_ptr node_id_expr =
      Constant::build(PrimitiveType::PrimitiveKind::INT16_T, node_id);
  Type_ptr void_ret_type =
      PrimitiveType::build(PrimitiveType::PrimitiveKind::VOID);
  FunctionCall_ptr inc_path_counter =
      FunctionCall::build("inc_path_counter", {node_id_expr}, void_ret_type);
  inc_path_counter->set_terminate_line(true);
  return inc_path_counter;
}

Node_ptr build_ast(AST &ast, const bdd::Node *root, TargetOption target) {
  std::vector<Node_ptr> nodes;

  Variable_ptr dst_device_var = ast.get_from_local("dst_device");
  if (!dst_device_var) {
    dst_device_var = Variable::build(
        "dst_device",
        PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT16_T));
    VariableDecl_ptr dst_device_var_decl = VariableDecl::build(dst_device_var);
    dst_device_var_decl->set_terminate_line(true);
    ast.push_to_local(dst_device_var);
    nodes.push_back(dst_device_var_decl);
  }

  while (root) {
    bdd::PrinterDebug::debug(root);
    std::cerr << "\n";

    if (target == BDD_PATH_PROFILER) {
      auto hit_rate_update = update_path_profiler_rate(ast, root->get_id());
      nodes.push_back(hit_rate_update);
    }

    switch (root->get_type()) {
    case bdd::NodeType::BRANCH: {
      auto branch_node = static_cast<const bdd::Branch *>(root);

      auto on_true_bdd = branch_node->get_on_true();
      auto on_false_bdd = branch_node->get_on_false();

      auto cond = branch_node->get_condition();

      ast.push();
      auto then_node = build_ast(ast, on_true_bdd, target);
      ast.pop();

      ast.push();
      auto else_node = build_ast(ast, on_false_bdd, target);
      ast.pop();

      auto condition = transpile(&ast, cond, true);

      auto on_true_term =
          on_true_bdd ? on_true_bdd->get_leaves() : std::vector<uint64_t>();
      auto on_false_term =
          on_false_bdd ? on_false_bdd->get_leaves() : std::vector<uint64_t>();

      Branch_ptr branch = Branch::build(condition, then_node, else_node,
                                        on_true_term, on_false_term);
      nodes.push_back(branch);
      return Block::build(nodes);
    } break;
    case bdd::NodeType::CALL: {
      auto bdd_call = static_cast<const bdd::Call *>(root);
      auto call_node = ast.node_from_call(bdd_call, target);

      if (call_node) {
        nodes.push_back(call_node);
      }

      root = root->get_next();
    } break;
    case bdd::NodeType::ROUTE: {
      const bdd::Route *route = static_cast<const bdd::Route *>(root);

      Expr_ptr dst_device;
      bdd::RouteOperation op = route->get_operation();

      switch (op) {
      case bdd::RouteOperation::FWD: {
        Comment_ptr comm = Comment::build("forward");
        dst_device = Constant::build(PrimitiveType::PrimitiveKind::INT,
                                     route->get_dst_device());
      } break;
      case bdd::RouteOperation::BCAST: {
        Comment_ptr comm = Comment::build("broadcast");
        dst_device = Variable::build(
            "FLOOD",
            PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT16_T));
      } break;
      case bdd::RouteOperation::DROP: {
        Comment_ptr comm = Comment::build("drop");
        dst_device = Variable::build(
            "DROP",
            PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT16_T));
      } break;
      }

      Expr_ptr new_node = Assignment::build(dst_device_var, dst_device);
      new_node->set_terminate_line(true);
      nodes.push_back(new_node);

      root = root->get_next();
    } break;
    }
  }

  Return_ptr ret = Return::build(dst_device_var);
  nodes.push_back(ret);

  assert(nodes.size());
  return Block::build(nodes);
}

Block_ptr build_init_block(AST &ast, const bdd::BDD &bdd, TargetOption target) {
  std::vector<Node_ptr> nodes;

  Constant_ptr zero = Constant::build(PrimitiveType::PrimitiveKind::INT, 0);
  Constant_ptr one = Constant::build(PrimitiveType::PrimitiveKind::INT, 1);

  Return_ptr ret_fail = Return::build(zero);
  Return_ptr ret_success = Return::build(one);

  const calls_t &calls = bdd.get_init();
  for (const call_t &call : calls) {
    FunctionCall_ptr init_call_node = ast.init_node_from_call(call, target);
    Equals_ptr call_failed = Equals::build(init_call_node, zero);
    Branch_ptr branch = Branch::build(call_failed, ret_fail, nullptr);
    nodes.push_back(branch);
  }

  if (target == BDD_PATH_PROFILER) {
    size_t total_code_paths = bdd.size();

    auto u64 = PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT64_T);
    auto path_profiler_counter_type = Array::build(u64, total_code_paths);
    auto path_profiler_counter =
        Variable::build("path_profiler_counter", path_profiler_counter_type);
    ast.push_to_state(path_profiler_counter);

    for (size_t i = 0; i < total_code_paths; i++) {
      auto byte = PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT8_T);
      auto idx = Constant::build(PrimitiveType::PrimitiveKind::INT, i);
      auto path_profiler_counter_byte =
          Read::build(path_profiler_counter, byte, idx);
      auto assignment = Assignment::build(
          path_profiler_counter_byte,
          Constant::build(PrimitiveType::PrimitiveKind::INT, 0));
      assignment->set_terminate_line(true);
      nodes.push_back(assignment);
    }

    auto u64_ptr = Pointer::build(u64);
    auto u32 = PrimitiveType::build(PrimitiveType::PrimitiveKind::UINT32_T);

    auto path_profiler_counter_ptr =
        Variable::build("path_profiler_counter_ptr", u64_ptr);
    auto path_profiler_counter_sz =
        Variable::build("path_profiler_counter_sz", u32);
    auto call_paths_sz = Constant::build(PrimitiveType::PrimitiveKind::UINT32_T,
                                         total_code_paths);

    auto path_profiler_counter_ptr_val =
        Assignment::build(path_profiler_counter_ptr, path_profiler_counter);
    auto path_profiler_counter_sz_val =
        Assignment::build(path_profiler_counter_sz, call_paths_sz);

    path_profiler_counter_ptr_val->set_terminate_line(true);
    path_profiler_counter_sz_val->set_terminate_line(true);

    nodes.push_back(path_profiler_counter_ptr_val);
    nodes.push_back(path_profiler_counter_sz_val);
  }

  nodes.push_back(ret_success);

  return Block::build(nodes, false);
}

Block_ptr build_process_block(AST &ast, const bdd::BDD &bdd,
                              TargetOption target) {
  const bdd::Node *root = bdd.get_root();
  Node_ptr process_root = build_ast(ast, root, target);
  return Block::build(process_root, false);
}

void build_ast(AST &ast, const bdd::BDD &bdd, TargetOption target) {
  Block_ptr init_block = build_init_block(ast, bdd, target);
  ast.commit(init_block);

  Block_ptr process_block = build_process_block(ast, bdd, target);
  ast.commit(process_block);

  const std::vector<Variable_ptr> &state = ast.get_state();
  for (Variable_ptr gv : state) {
    VariableDecl_ptr decl = VariableDecl::build(gv);
    decl->set_terminate_line(true);
    ast.push_global_code(decl);
  }
}

void synthesize(const AST &ast, TargetOption target, std::ostream &out) {
  // Not very OS friendly, but oh well...
  auto source_file = std::string(__FILE__);
  auto boilerplate_path =
      source_file.substr(0, source_file.rfind("/")) + "/boilerplates/";

  switch (target) {
  case SEQUENTIAL: {
    boilerplate_path += "sequential.template.cpp";
    break;
  }
  case BDD_PATH_PROFILER: {
    boilerplate_path += "bdd-analyzer.template.cpp";
    break;
  }
  default: {
    assert(false && "No boilerplate for this target");
  }
  }

  std::ifstream boilerplate(boilerplate_path, std::ios::in);
  assert(!boilerplate.fail() && "Boilerplate file not found");

  out << boilerplate.rdbuf();
  ast.print(out);
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  bdd::BDD bdd(InputBDDFile);

  AST ast;
  build_ast(ast, bdd, Target);

  if (Out.size()) {
    auto file = std::ofstream(Out);
    assert(file.is_open());
    synthesize(ast, Target, file);
  } else {
    synthesize(ast, Target, std::cout);
  }

  if (XML.size()) {
    auto file = std::ofstream(XML);
    assert(file.is_open());
    ast.print_xml(file);
  }

  return 0;
}
