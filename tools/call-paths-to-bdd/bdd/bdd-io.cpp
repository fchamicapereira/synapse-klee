#include "bdd-io.h"
#include "bdd.h"

#include "nodes/return_init.h"
#include "nodes/return_process.h"

#include "llvm/Support/MemoryBuffer.h"

#include <fstream>
#include <iostream>

namespace BDD {

std::vector<std::string> skip_conditions_with_symbol{"received_a_packet",
                                                     "loop_termination"};

struct kQuery_t {
  std::vector<const klee::Array *> arrays;
  std::vector<std::string> exprs;

  std::string serialize() {
    std::stringstream stream;

    for (auto array : arrays) {
      stream << "array";
      stream << " ";
      stream << array->getName();
      stream << "[" << array->getSize() << "]";
      stream << " : ";
      stream << "w" << array->getDomain();
      stream << " -> ";
      stream << "w" << array->getRange();
      stream << " = ";
      stream << "symbolic";
      stream << "\n";
    }

    stream << "(query [] false [\n";
    for (auto expr : exprs) {
      stream << "       ";
      stream << expr;
      stream << "\n";
    }
    stream << "   ])\n";

    return stream.str();
  }
};

void fill_arrays(klee::ref<klee::Expr> expr,
                 std::vector<const klee::Array *> &arrays) {
  kutil::RetrieveSymbols retriever;
  retriever.visit(expr);

  auto reads = retriever.get_retrieved();
  for (auto read : reads) {
    auto updates = read->updates;
    auto root = updates.root;

    assert(root->isSymbolicArray());
    auto found_it = std::find_if(arrays.begin(), arrays.end(),
                                 [&](const klee::Array *array) {
                                   return array->getName() == root->getName();
                                 });

    if (found_it == arrays.end()) {
      arrays.push_back(root);
    }
  }
}

std::string serialize_expr(klee::ref<klee::Expr> expr, kQuery_t &kQuery) {
  assert(!expr.isNull());
  fill_arrays(expr, kQuery.arrays);

  auto expr_str = kutil::expr_to_string(expr);

  while (1) {
    auto delim = expr_str.find(":");

    if (delim == std::string::npos) {
      break;
    }

    auto start = delim;
    auto end = delim;

    while (expr_str[--start] != 'N') {
      assert(start > 0 && start < expr_str.size());
    }

    auto pre = expr_str.substr(0, start);
    auto post = expr_str.substr(end + 1);

    auto label_name = expr_str.substr(start, end - start);
    auto label_expr = std::string();

    expr_str = pre + post;

    auto parenthesis_lvl = 0;

    for (auto c : post) {
      if (c == '(') {
        parenthesis_lvl++;
      } else if (c == ')') {
        parenthesis_lvl--;
      }

      label_expr += c;

      if (parenthesis_lvl == 0) {
        break;
      }
    }

    while (1) {
      delim = expr_str.find(label_name);

      if (delim == std::string::npos) {
        break;
      }

      auto label_sz = label_name.size();

      if (delim + label_sz < expr_str.size() &&
          expr_str[delim + label_sz] == ':') {
        pre = expr_str.substr(0, delim);
        post = expr_str.substr(delim + label_sz + 1);

        expr_str = pre + post;
        continue;
      }

      pre = expr_str.substr(0, delim);
      post = expr_str.substr(delim + label_sz);

      expr_str = pre + label_expr + post;
    }
  }

  kQuery.exprs.push_back(expr_str);

  return expr_str;
}

std::string serialize_call(const call_t &call, kQuery_t &kQuery) {
  std::stringstream call_stream;
  std::string expr_str;

  call_stream << call.function_name;
  call_stream << "(";

  auto first = true;
  for (auto arg_pair : call.args) {
    auto arg_name = arg_pair.first;
    auto arg = arg_pair.second;

    if (first) {
      first = false;
    } else {
      call_stream << ",";
    }

    call_stream << arg_name;
    call_stream << ":";

    expr_str = serialize_expr(arg.expr, kQuery);
    call_stream << expr_str;

    if (arg.fn_ptr_name.first) {
      call_stream << "&";
      call_stream << arg.fn_ptr_name.second;
      continue;
    }

    if (arg.in.isNull() && arg.out.isNull()) {
      continue;
    }

    call_stream << "&[";

    if (!arg.in.isNull()) {
      expr_str = serialize_expr(arg.in, kQuery);
      call_stream << expr_str;
    }

    call_stream << "->";

    if (!arg.out.isNull()) {
      expr_str = serialize_expr(arg.out, kQuery);
      call_stream << expr_str;
    }

    call_stream << "]";

    call_stream << "[";
    for (auto i = 0u; i < arg.meta.size(); i++) {
      auto meta = arg.meta[i];

      if (i != 0) {
        call_stream << ",";
      }

      call_stream << "{";
      call_stream << meta.symbol;
      call_stream << ",";
      call_stream << meta.offset;
      call_stream << ",";
      call_stream << meta.size;
      call_stream << "}";
    }
    call_stream << "]";
  }

  call_stream << ")";

  if (call.extra_vars.size()) {
    auto first = true;

    call_stream << "{";

    for (auto extra_var_pair : call.extra_vars) {
      auto extra_var_name = extra_var_pair.first;
      auto extra_var = extra_var_pair.second;

      if (first) {
        first = false;
      } else {
        call_stream << ",";
      }

      call_stream << extra_var_name;
      call_stream << ":";

      auto in = extra_var.first;
      auto out = extra_var.second;

      call_stream << "[";

      if (!in.isNull()) {
        expr_str = serialize_expr(in, kQuery);
        call_stream << expr_str;
      }

      call_stream << "->";

      if (!out.isNull()) {
        expr_str = serialize_expr(out, kQuery);
        call_stream << expr_str;
      }

      call_stream << "]";
    }

    call_stream << "}";
  }

  call_stream << "->";

  if (call.ret.isNull()) {
    call_stream << "[]";
  } else {
    expr_str = serialize_expr(call.ret, kQuery);
    call_stream << expr_str;
  }

  return call_stream.str();
}

void BDD::serialize(std::string out_file) const {
  std::ofstream out(out_file);

  assert(out);
  assert(out.is_open());

  kQuery_t kQuery;

  std::stringstream kQuery_stream;
  std::stringstream kQuery_cp_constraints_stream;

  std::stringstream nodes_stream;
  std::stringstream edges_stream;

  std::vector<const Node *> nodes{nf_init.get(), nf_process.get()};

  while (nodes.size()) {
    auto node = nodes[0];
    nodes.erase(nodes.begin());

    nodes_stream << "\n";

    nodes_stream << node->get_id();
    nodes_stream << ":(";

    auto manager = node->get_node_constraints();

    nodes_stream << manager.size();
    nodes_stream << " ";

    for (auto constraint : manager) {
      serialize_expr(constraint, kQuery);
    }

    switch (node->get_type()) {
    case Node::NodeType::CALL: {
      auto call_node = static_cast<const Call *>(node);

      nodes_stream << "CALL";
      nodes_stream << " ";
      nodes_stream << serialize_call(call_node->get_call(), kQuery);

      assert(node->get_next());

      edges_stream << "\n";
      edges_stream << "(";
      edges_stream << node->get_id();
      edges_stream << "->";
      edges_stream << node->get_next()->get_id();
      edges_stream << ")";

      nodes.push_back(node->get_next().get());
      break;
    }
    case Node::NodeType::BRANCH: {
      auto branch_node = static_cast<const Branch *>(node);
      auto condition = branch_node->get_condition();

      assert(!condition.isNull());

      nodes_stream << "BRANCH";
      nodes_stream << " ";
      nodes_stream << serialize_expr(condition, kQuery);

      assert(branch_node->get_on_true());
      assert(branch_node->get_on_false());

      edges_stream << "\n";
      edges_stream << "(";
      edges_stream << node->get_id();
      edges_stream << "->";
      edges_stream << branch_node->get_on_true()->get_id();
      edges_stream << "->";
      edges_stream << branch_node->get_on_false()->get_id();
      edges_stream << ")";

      nodes.push_back(branch_node->get_on_true().get());
      nodes.push_back(branch_node->get_on_false().get());
      break;
    }
    case Node::NodeType::RETURN_INIT: {
      auto return_init_node = static_cast<const ReturnInit *>(node);

      nodes_stream << "RETURN_INIT";
      nodes_stream << " ";
      switch (return_init_node->get_return_value()) {
      case ReturnInit::ReturnType::SUCCESS:
        nodes_stream << "SUCCESS";
        break;
      case ReturnInit::ReturnType::FAILURE:
        nodes_stream << "FAILURE";
        break;
      }

      assert(!node->get_next());
      break;
    }
    case Node::NodeType::RETURN_PROCESS: {
      auto return_process_node = static_cast<const ReturnProcess *>(node);

      nodes_stream << "RETURN_PROCESS";
      nodes_stream << " ";

      switch (return_process_node->get_return_operation()) {
      case ReturnProcess::Operation::FWD:
        nodes_stream << "FWD";
        break;
      case ReturnProcess::Operation::DROP:
        nodes_stream << "DROP";
        break;
      case ReturnProcess::Operation::ERR:
        nodes_stream << "ERR";
        break;
      case ReturnProcess::Operation::BCAST:
        nodes_stream << "BCAST";
        break;
      }

      nodes_stream << " ";
      nodes_stream << return_process_node->get_return_value();

      assert(!node->get_next());
      break;
    }
    case Node::NodeType::RETURN_RAW: {
      assert(false);
    }
    }

    nodes_stream << ")";
  }

  nodes_stream << "\n";
  edges_stream << "\n";

  out << MAGIC_SIGNATURE << "\n";

  out << ";;-- kQuery --\n";
  out << kQuery.serialize();

  out << ";; -- Nodes --";
  out << nodes_stream.str();

  out << ";; -- Edges --";
  out << edges_stream.str();

  out << ";; -- Roots --\n";
  out << "init:" << nf_init.get()->get_id() << "\n";
  out << "process:" << nf_process.get()->get_id() << "\n";

  out.close();
}

klee::ref<klee::Expr> pop_expr(std::vector<klee::ref<klee::Expr>> &exprs) {
  auto expr = exprs[0];
  exprs.erase(exprs.begin());
  return expr;
}

std::vector<meta_t> parse_meta(const std::string &meta_str) {
  std::vector<meta_t> meta;
  std::vector<std::string> elems(3);

  auto lvl = 0;
  auto curr_el = 0;

  for (auto c : meta_str) {
    if (c == '{') {
      lvl++;
      continue;
    }

    if (c == '}') {
      lvl--;

      auto symbol = elems[0];
      auto offset = static_cast<bits_t>(std::stoi(elems[1]));
      auto size = static_cast<bits_t>(std::stoi(elems[2]));

      auto m = meta_t{symbol, offset, size};
      meta.push_back(m);

      continue;
    }

    if (c == ',' && lvl == 0) {
      for (auto &el : elems) {
        el.clear();
      }

      curr_el = 0;
      continue;
    }

    else if (c == ',') {
      curr_el++;
      continue;
    }

    elems[curr_el] += c;
  }

  return meta;
}

std::pair<std::string, arg_t>
parse_arg(std::string serialized_arg,
          std::vector<klee::ref<klee::Expr>> &exprs) {
  std::string arg_name;
  arg_t arg;

  auto delim = serialized_arg.find(":");
  assert(delim != std::string::npos);

  arg_name = serialized_arg.substr(0, delim);
  serialized_arg = serialized_arg.substr(delim + 1);

  std::string expr_str;
  std::string in_str;
  std::string out_str;
  std::string fn_ptr_name;
  std::string meta_str;

  delim = serialized_arg.find("&");

  if (delim == std::string::npos) {
    expr_str = serialized_arg;
  } else {
    expr_str = serialized_arg.substr(0, delim);
    serialized_arg = serialized_arg.substr(delim + 1);

    delim = serialized_arg.find("[");

    if (delim == std::string::npos) {
      fn_ptr_name = serialized_arg;
    } else {
      serialized_arg = serialized_arg.substr(delim + 1);

      delim = serialized_arg.find("->");
      assert(delim != std::string::npos);

      in_str = serialized_arg.substr(0, delim);

      serialized_arg = serialized_arg.substr(delim + 2);

      delim = serialized_arg.find("]");
      assert(delim != std::string::npos);

      out_str = serialized_arg.substr(0, delim);
      meta_str = serialized_arg.substr(delim + 1);

      auto meta_start = meta_str.find("[");
      auto meta_end = meta_str.find("]");

      assert(meta_start != std::string::npos);
      assert(meta_end != std::string::npos);

      meta_str = meta_str.substr(meta_start + 1, meta_end - 1);
      arg.meta = parse_meta(meta_str);
    }
  }

  if (expr_str.size()) {
    arg.expr = pop_expr(exprs);
  }

  if (fn_ptr_name.size()) {
    arg.fn_ptr_name = std::make_pair(true, fn_ptr_name);
  }

  if (in_str.size()) {
    arg.in = pop_expr(exprs);
  }

  if (out_str.size()) {
    arg.out = pop_expr(exprs);
  }

  return std::make_pair(arg_name, arg);
}

std::pair<std::string, std::pair<klee::ref<klee::Expr>, klee::ref<klee::Expr>>>
parse_extra_var(std::string serialized_extra_var,
                std::vector<klee::ref<klee::Expr>> &exprs) {
  std::string extra_var_name;
  klee::ref<klee::Expr> in;
  klee::ref<klee::Expr> out;

  auto delim = serialized_extra_var.find(":");
  assert(delim != std::string::npos);

  extra_var_name = serialized_extra_var.substr(0, delim);
  serialized_extra_var = serialized_extra_var.substr(delim + 1);

  std::string in_str;
  std::string out_str;

  delim = serialized_extra_var.find("[");
  assert(delim != std::string::npos);

  serialized_extra_var = serialized_extra_var.substr(delim + 1);

  delim = serialized_extra_var.find("->");
  assert(delim != std::string::npos);

  in_str = serialized_extra_var.substr(0, delim);
  out_str = serialized_extra_var.substr(delim + 2);

  delim = out_str.find("]");
  assert(delim != std::string::npos);

  out_str = out_str.substr(0, delim);

  if (in_str.size()) {
    in = pop_expr(exprs);
  }

  if (out_str.size()) {
    out = pop_expr(exprs);
  }

  return std::make_pair(extra_var_name, std::make_pair(in, out));
}

call_t parse_call(std::string serialized_call,
                  std::vector<klee::ref<klee::Expr>> &exprs) {
  call_t call;

  // cleanup by removing duplicated spaces
  auto new_end =
      std::unique(serialized_call.begin(), serialized_call.end(),
                  [](char lhs, char rhs) { return lhs == rhs && lhs == ' '; });
  serialized_call.erase(new_end, serialized_call.end());

  auto delim = serialized_call.find("(");
  assert(delim != std::string::npos);

  call.function_name = serialized_call.substr(0, delim);
  serialized_call = serialized_call.substr(delim + 1);

  std::vector<std::string> args_str;

  int parenthesis_lvl = 1;
  delim = 0;
  std::string arg_str;
  for (auto c : serialized_call) {
    delim++;
    if (c == '(' || c == '[') {
      parenthesis_lvl++;
    } else if (c == ')' || c == ']') {
      parenthesis_lvl--;

      if (parenthesis_lvl == 0) {
        if (arg_str.size()) {
          args_str.push_back(arg_str);
          arg_str.clear();
        }
        break;
      }
    } else if (c == ',' && parenthesis_lvl == 1) {
      args_str.push_back(arg_str);
      arg_str.clear();

      continue;
    }

    arg_str += c;
  }

  for (auto arg_str : args_str) {
    auto arg_pair = parse_arg(arg_str, exprs);
    call.args[arg_pair.first] = arg_pair.second;
  }

  serialized_call = serialized_call.substr(delim);
  delim = serialized_call.find("{");

  if (delim != std::string::npos) {
    serialized_call = serialized_call.substr(1);
    std::vector<std::string> extra_vars_str;

    delim = 0;
    std::string extra_var_str;
    for (auto c : serialized_call) {
      delim++;

      if (c == ',') {
        extra_vars_str.push_back(extra_var_str);
        extra_var_str.clear();
        continue;
      } else if (c == '}') {
        extra_vars_str.push_back(extra_var_str);
        extra_var_str.clear();
        break;
      }

      extra_var_str += c;
    }

    for (auto extra_var_str : extra_vars_str) {
      auto extra_var_pair = parse_extra_var(extra_var_str, exprs);
      call.extra_vars[extra_var_pair.first] = extra_var_pair.second;
    }

    serialized_call = serialized_call.substr(delim);
  }

  delim = serialized_call.find("->");
  assert(delim != std::string::npos);

  serialized_call = serialized_call.substr(delim + 2);

  if (serialized_call != "[]") {
    call.ret = pop_expr(exprs);
  }

  return call;
}

Node_ptr parse_node_call(node_id_t id,
                         const klee::ConstraintManager &constraints,
                         std::string serialized,
                         std::vector<klee::ref<klee::Expr>> &exprs) {
  auto call = parse_call(serialized, exprs);
  auto call_node =
      std::make_shared<Call>(id, nullptr, nullptr, constraints, call);
  return call_node;
}

Node_ptr parse_node_branch(node_id_t id,
                           const klee::ConstraintManager &constraints,
                           std::string serialized,
                           std::vector<klee::ref<klee::Expr>> &exprs) {
  auto condition = pop_expr(exprs);
  auto branch_node = std::make_shared<Branch>(id, nullptr, nullptr, constraints,
                                              nullptr, condition);
  return branch_node;
}

Node_ptr parse_node_return_init(node_id_t id,
                                const klee::ConstraintManager &constraints,
                                std::string serialized,
                                std::vector<klee::ref<klee::Expr>> &exprs) {
  auto return_init_str = serialized;
  ReturnInit::ReturnType return_value;

  if (return_init_str == "SUCCESS") {
    return_value = ReturnInit::ReturnType::SUCCESS;
  } else if (return_init_str == "FAILURE") {
    return_value = ReturnInit::ReturnType::FAILURE;
  } else {
    assert(false);
  }

  auto return_init_node =
      std::make_shared<ReturnInit>(id, nullptr, constraints, return_value);
  return return_init_node;
}

Node_ptr parse_node_return_process(node_id_t id,
                                   const klee::ConstraintManager &constraints,
                                   std::string serialized,
                                   std::vector<klee::ref<klee::Expr>> &exprs) {
  auto delim = serialized.find(" ");
  assert(delim != std::string::npos);

  auto return_operation_str = serialized.substr(0, delim);
  auto return_value_str = serialized.substr(delim + 1);

  ReturnProcess::Operation return_operation;
  int return_value;

  if (return_operation_str == "FWD") {
    return_operation = ReturnProcess::Operation::FWD;
  } else if (return_operation_str == "DROP") {
    return_operation = ReturnProcess::Operation::DROP;
  } else if (return_operation_str == "BCAST") {
    return_operation = ReturnProcess::Operation::BCAST;
  } else if (return_operation_str == "ERR") {
    return_operation = ReturnProcess::Operation::ERR;
  } else {
    assert(false);
  }

  return_value = std::stoi(return_value_str);

  auto return_process_node = std::make_shared<ReturnProcess>(
      id, nullptr, constraints, return_value, return_operation);

  return return_process_node;
}

Node_ptr parse_node(std::string serialized_node,
                    std::vector<klee::ref<klee::Expr>> &exprs) {
  Node_ptr node;

  auto delim = serialized_node.find(":");
  assert(delim != std::string::npos);

  auto id = std::stoull(serialized_node.substr(0, delim));
  serialized_node = serialized_node.substr(delim + 1);

  assert(serialized_node[0] == '(');
  serialized_node = serialized_node.substr(1);

  delim = serialized_node.find(" ");
  assert(delim != std::string::npos);

  auto serialized_constraints_num = serialized_node.substr(0, delim);

  serialized_node = serialized_node.substr(delim + 1);

  auto constraints_num = std::atoi(serialized_constraints_num.c_str());
  assert(constraints_num >= 0);

  klee::ConstraintManager manager;

  for (auto i = 0; i < constraints_num; i++) {
    auto constraint = pop_expr(exprs);
    manager.addConstraint(constraint);
  }

  delim = serialized_node.find(" ");
  assert(delim != std::string::npos);

  auto node_type_str = serialized_node.substr(0, delim);

  serialized_node = serialized_node.substr(delim + 1);
  serialized_node = serialized_node.substr(0, serialized_node.size() - 1);

  if (node_type_str == "CALL") {
    node = parse_node_call(id, manager, serialized_node, exprs);
  } else if (node_type_str == "BRANCH") {
    node = parse_node_branch(id, manager, serialized_node, exprs);
  } else if (node_type_str == "RETURN_INIT") {
    node = parse_node_return_init(id, manager, serialized_node, exprs);
  } else if (node_type_str == "RETURN_PROCESS") {
    node = parse_node_return_process(id, manager, serialized_node, exprs);
  } else {
    assert(false);
  }

  assert(node);

  return node;
}

void parse_kQuery(std::string kQuery,
                  std::vector<klee::ref<klee::Expr>> &exprs) {
  llvm::MemoryBuffer *MB = llvm::MemoryBuffer::getMemBuffer(kQuery);
  klee::ExprBuilder *Builder = klee::createDefaultExprBuilder();
  klee::expr::Parser *P = klee::expr::Parser::Create("", MB, Builder, false);

  while (klee::expr::Decl *D = P->ParseTopLevelDecl()) {
    assert(!P->GetNumErrors() && "Error parsing kquery in BDD file.");

    if (auto *QC = dyn_cast<klee::expr::QueryCommand>(D)) {
      exprs = QC->Values;
      break;
    }
  }
}

void process_edge(std::string serialized_edge,
                  std::map<node_id_t, Node_ptr> &nodes) {
  auto delim = serialized_edge.find("(");
  assert(delim != std::string::npos);

  serialized_edge = serialized_edge.substr(delim + 1);

  delim = serialized_edge.find(")");
  assert(delim != std::string::npos);

  serialized_edge = serialized_edge.substr(0, delim);

  delim = serialized_edge.find("->");
  assert(delim != std::string::npos);

  auto prev_id_str = serialized_edge.substr(0, delim);
  auto prev_id = std::stoi(prev_id_str);

  assert(nodes.find(prev_id) != nodes.end());
  auto prev = nodes[prev_id];

  serialized_edge = serialized_edge.substr(delim + 2);

  delim = serialized_edge.find("->");

  if (delim != std::string::npos) {
    assert(prev->get_type() == Node::NodeType::BRANCH);

    auto on_true_id_str = serialized_edge.substr(0, delim);
    auto on_false_id_str = serialized_edge.substr(delim + 2);

    auto on_true_id = std::stoi(on_true_id_str);
    auto on_false_id = std::stoi(on_false_id_str);

    assert(nodes.find(on_true_id) != nodes.end());
    assert(nodes.find(on_false_id) != nodes.end());

    auto on_true = nodes[on_true_id];
    auto on_false = nodes[on_false_id];

    auto branch_node = static_cast<Branch *>(prev.get());

    branch_node->replace_on_true(on_true);
    branch_node->replace_on_false(on_false);

    on_true->replace_prev(prev);
    on_false->replace_prev(prev);
  } else {
    assert(prev->get_type() == Node::NodeType::CALL);

    auto next_id_str = serialized_edge;
    auto next_id = std::stoi(next_id_str);

    assert(nodes.find(next_id) != nodes.end());
    auto next = nodes[next_id];

    prev->replace_next(next);
    next->replace_prev(prev);
  }
}

void BDD::deserialize(const std::string &file_path) {
  auto magic_check = false;

  std::ifstream bdd_file(file_path);

  if (!bdd_file.is_open()) {
    std::cerr << "Unable to open BDD file.\n";
    exit(1);
  }

  enum {
    STATE_INIT,
    STATE_KQUERY,
    STATE_NODES,
    STATE_EDGES,
    STATE_ROOTS,
  } state = STATE_INIT;

  auto get_next_state = [&](std::string line) {
    if (line == ";;-- kQuery --") {
      return STATE_KQUERY;
    }
    if (line == ";; -- Nodes --") {
      return STATE_NODES;
    }
    if (line == ";; -- Edges --") {
      return STATE_EDGES;
    }
    if (line == ";; -- Roots --") {
      return STATE_ROOTS;
    }

    return state;
  };

  std::string kQuery;

  std::vector<klee::ref<klee::Expr>> exprs;
  std::map<node_id_t, Node_ptr> nodes;

  int parenthesis_level = 0;
  std::string current_node;
  std::string current_call_path;

  while (!bdd_file.eof()) {
    std::string line;
    std::getline(bdd_file, line);

    if (line.size() == 0) {
      continue;
    }

    switch (state) {
    case STATE_INIT: {
      if (line == MAGIC_SIGNATURE) {
        magic_check = true;
      }

      break;
    }

    case STATE_KQUERY: {
      kQuery += line + "\n";

      if (get_next_state(line) != state) {
        parse_kQuery(kQuery, exprs);
      }
    } break;

    case STATE_NODES: {
      if (get_next_state(line) != state) {
        break;
      }

      current_node += line;

      for (auto c : line) {
        if (c == '(') {
          parenthesis_level++;
        } else if (c == ')') {
          parenthesis_level--;
        }
      }

      if (parenthesis_level == 0) {
        auto node = parse_node(current_node, exprs);

        assert(node);
        assert(nodes.find(node->get_id()) == nodes.end());

        id = std::max(id, node->get_id()) + 1;

        nodes[node->get_id()] = node;
        current_node.clear();
      }
    } break;

    case STATE_EDGES: {
      if (get_next_state(line) != state) {
        break;
      }

      process_edge(line, nodes);
    } break;

    case STATE_ROOTS: {
      if (get_next_state(line) != state) {
        break;
      }

      auto delim = line.find(":");
      assert(delim != std::string::npos);

      auto root_type = line.substr(0, delim);
      auto root_id_str = line.substr(delim + 1);

      if (root_type == "init") {
        node_id_t init_id = std::stoll(root_id_str);
        assert(nodes.find(init_id) != nodes.end());

        nf_init = std::shared_ptr<Node>(nodes[init_id]);
        break;
      } else if (root_type == "process") {
        node_id_t process_id = std::stoll(root_id_str);
        assert(nodes.find(process_id) != nodes.end());

        nf_process = std::shared_ptr<Node>(nodes[process_id]);
        break;
      } else {
        assert(false);
      }
    } break;

    default: {
      // ignore
    }
    }

    if (state == STATE_INIT && get_next_state(line) != state && !magic_check) {
      std::cerr << "\"" << file_path << "\" not a BDD file. Aborting.\n";
      exit(1);
    }

    state = get_next_state(line);
  }

  if (!magic_check) {
    std::cerr << "\"" << file_path << "\" not a BDD file. Aborting.\n";
    exit(1);
  }
}

} // namespace BDD