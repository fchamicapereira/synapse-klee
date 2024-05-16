#pragma once

#include <assert.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <math.h>
#include <unistd.h>

#include "call-paths-to-bdd.h"

namespace bdd {

struct color_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t o;

  color_t(uint8_t _r, uint8_t _g, uint8_t _b) : r(_r), g(_g), b(_b), o(0xff) {}

  color_t(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _o)
      : r(_r), g(_g), b(_b), o(_o) {}

  std::string to_gv_repr() const {
    std::stringstream ss;

    ss << "#";
    ss << std::hex;

    ss << std::setw(2);
    ss << std::setfill('0');
    ss << (int)r;

    ss << std::setw(2);
    ss << std::setfill('0');
    ss << (int)g;

    ss << std::setw(2);
    ss << std::setfill('0');
    ss << (int)b;

    ss << std::setw(2);
    ss << std::setfill('0');
    ss << (int)o;

    ss << std::dec;

    return ss.str();
  }
};

struct bdd_visualizer_opts_t {
  std::unordered_map<node_id_t, std::string> colors_per_node;
  std::pair<bool, std::string> default_color;
  std::unordered_map<node_id_t, std::string> annotations_per_node;

  struct processed_t {
    std::unordered_set<node_id_t> nodes;
    const Node *next;

    processed_t() : next(nullptr) {}
  } processed;

  bdd_visualizer_opts_t() {}
};

class GraphvizGenerator : public BDDVisitor {
private:
  std::ostream &os;
  bdd_visualizer_opts_t opts;

  const char *COLOR_PROCESSED = "gray";
  const char *COLOR_NEXT = "cyan";

  const char *COLOR_CALL = "cornflowerblue";
  const char *COLOR_BRANCH = "yellow";
  const char *COLOR_RETURN_INIT_SUCCESS = "chartreuse2";
  const char *COLOR_RETURN_INIT_FAILURE = "brown1";
  const char *COLOR_RETURN_PROCESS_FORWARD = "chartreuse2";
  const char *COLOR_RETURN_PROCESS_DROP = "brown1";
  const char *COLOR_RETURN_PROCESS_BCAST = "purple";

public:
  GraphvizGenerator(std::ostream &_os, const bdd_visualizer_opts_t &_opts)
      : os(_os), opts(_opts) {}

  GraphvizGenerator(std::ostream &_os) : os(_os) {}

  void set_opts(const bdd_visualizer_opts_t &_opts) { opts = _opts; }

  bool has_color(node_id_t id) const {
    return opts.colors_per_node.find(id) != opts.colors_per_node.end();
  }

  std::string get_color(const Node *node) const {
    assert(node);
    node_id_t id = node->get_id();

    if (has_color(id)) {
      return opts.colors_per_node.at(id);
    }

    if (opts.default_color.first) {
      return opts.default_color.second;
    }

    if (opts.processed.nodes.find(id) != opts.processed.nodes.end()) {
      return COLOR_PROCESSED;
    }

    if (opts.processed.next && id == opts.processed.next->get_id()) {
      return COLOR_NEXT;
    }

    std::string color;

    switch (node->get_type()) {
    case NodeType::CALL:
      color = COLOR_CALL;
      break;
    case NodeType::BRANCH:
      color = COLOR_BRANCH;
      break;
    case NodeType::ROUTE: {
      const Route *route = static_cast<const Route *>(node);
      Route::Operation operation = route->get_operation();
      switch (operation) {
      case Route::Operation::FWD:
        color = COLOR_RETURN_PROCESS_FORWARD;
        break;
      case Route::Operation::DROP:
        color = COLOR_RETURN_PROCESS_DROP;
        break;
      case Route::Operation::BCAST:
        color = COLOR_RETURN_PROCESS_BCAST;
        break;
      }
      break;
    }
    }

    return color;
  }

  static void visualize(const BDD &bdd, bool interrupt = true,
                        std::string file_path = "") {
    bdd_visualizer_opts_t opts;
    visualize(bdd, opts, interrupt, file_path);
  }

  static void visualize(const BDD &bdd, const bdd_visualizer_opts_t &opts,
                        bool interrupt = true, std::string file_path = "") {
    auto random_fname_generator = []() {
      constexpr int fname_len = 15;
      constexpr const char *prefix = "/tmp/";
      constexpr const char *alphanum = "0123456789"
                                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                       "abcdefghijklmnopqrstuvwxyz";

      std::stringstream ss;
      static unsigned counter = 1;

      ss << prefix;

      srand((unsigned)std::time(NULL) * getpid() + counter);

      for (int i = 0; i < fname_len; i++) {
        ss << alphanum[rand() % (strlen(alphanum) - 1)];
      }

      ss << ".gv";

      counter++;
      return ss.str();
    };

    auto open_graph = [](const std::string &fpath) {
      std::string file_path = __FILE__;
      std::string dir_path = file_path.substr(0, file_path.rfind("/"));
      std::string script = "open_graph.sh";
      std::string cmd = dir_path + "/" + script + " " + fpath;

      auto status = system(cmd.c_str());

      if (status < 0) {
        std::cout << "Error: " << strerror(errno) << '\n';
        assert(false && "Failed to open graph.");
      }
    };

    if (file_path.empty())
      file_path = random_fname_generator();

    std::ofstream file(file_path);
    assert(file.is_open());

    GraphvizGenerator gv(file, opts);
    gv.visit(bdd);

    file.close();

    std::cerr << "Visualizing BDD";
    std::cerr << " id=" << bdd.get_id();
    std::cerr << " hash=" << bdd.hash();
    std::cerr << " file=" << file_path;
    std::cerr << "\n";

    open_graph(file_path);

    if (interrupt) {
      std::cout << "Press Enter to continue ";
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
  }

  void visit(const BDD &bdd) override {
    const Node *root = bdd.get_root();

    os << "digraph mygraph {\n";
    os << "\tnode [shape=box, style=\"rounded,filled\", border=0];\n";
    visitRoot(root);
    os << "}";
  }

  Action visitBranch(const Branch *node) override {
    const Node *on_true = node->get_on_true();
    const Node *on_false = node->get_on_false();

    klee::ref<klee::Expr> condition = node->get_condition();

    if (on_true)
      on_true->visit(*this);
    if (on_false)
      on_false->visit(*this);

    os << "\t" << get_gv_name(node);
    os << " [shape=Mdiamond, label=\"";

    os << node->get_id() << ":";
    os << kutil::pretty_print_expr(condition);

    if (opts.annotations_per_node.find(node->get_id()) !=
        opts.annotations_per_node.end()) {
      os << "\\n" << opts.annotations_per_node.at(node->get_id());
    }

    os << "\"";

    os << ", fillcolor=\"" << get_color(node) << "\"";
    os << "];\n";

    if (on_true) {
      os << "\t" << get_gv_name(node);
      os << " -> ";
      os << get_gv_name(on_true);
      os << " [label=\"True\"];\n";
    }

    if (on_false) {
      os << "\t" << get_gv_name(node);
      os << " -> ";
      os << get_gv_name(on_false);
      os << " [label=\"False\"];\n";
    }

    return STOP;
  }

  Action visitCall(const Call *node) override {
    const call_t &call = node->get_call();
    node_id_t id = node->get_id();
    const Node *next = node->get_next();

    if (next) {
      next->visit(*this);
    }

    os << "\t" << get_gv_name(node);
    os << " [label=\"";
    os << id << ":";
    os << call.function_name;
    os << "(";

    size_t i = 0;
    for (const std::pair<std::string, arg_t> &pair : call.args) {
      if (call.args.size() > 1) {
        os << "\\l";
        os << std::string(2, ' ');
      }

      os << pair.first << ":";
      const arg_t &arg = pair.second;

      if (arg.fn_ptr_name.first) {
        os << arg.fn_ptr_name.second;
      } else {
        os << kutil::pretty_print_expr(arg.expr, false);

        if (!arg.in.isNull() || !arg.out.isNull()) {
          os << "[";

          if (!arg.in.isNull()) {
            os << kutil::pretty_print_expr(arg.in, false);
          }

          if (!arg.out.isNull() &&
              (arg.in.isNull() || !kutil::solver_toolbox.are_exprs_always_equal(
                                      arg.in, arg.out))) {
            os << " -> ";
            os << kutil::pretty_print_expr(arg.out, false);
          }

          os << "]";
        }
      }

      if (i != call.args.size() - 1) {
        os << ",";
      }

      i++;
    }

    os << ")";

    if (!call.ret.isNull()) {
      os << " -> " << kutil::pretty_print_expr(call.ret);
    }

    const symbols_t &symbols = node->get_locally_generated_symbols();
    if (symbols.size()) {
      os << "\\l=>{";
      bool first = true;
      for (const symbol_t &s : symbols) {
        if (!first) {
          os << ",";
        } else {
          first = false;
        }
        os << s.array->name;
      }
      os << "}";
    }

    if (opts.annotations_per_node.find(node->get_id()) !=
        opts.annotations_per_node.end()) {
      os << "\\l" << opts.annotations_per_node.at(node->get_id());
    }

    os << "\"";

    os << ", fillcolor=\"" << get_color(node) << "\"";
    os << "];\n";

    if (next) {
      os << "\t" << get_gv_name(node);
      os << " -> ";
      os << get_gv_name(next);
      os << ";\n";
    }

    return STOP;
  }

  Action visitRoute(const Route *node) override {
    node_id_t id = node->get_id();
    int dst_device = node->get_dst_device();
    Route::Operation operation = node->get_operation();
    const Node *next = node->get_next();

    if (next) {
      next->visit(*this);
    }

    os << "\t" << get_gv_name(node);
    os << " [label=\"";
    os << id << ":";

    switch (operation) {
    case Route::Operation::FWD: {
      os << "fwd(" << dst_device << ")";
      break;
    }
    case Route::Operation::DROP: {
      os << "drop()";
      break;
    }
    case Route::Operation::BCAST: {
      os << "bcast()";
      break;
    }
    }

    if (opts.annotations_per_node.find(id) != opts.annotations_per_node.end()) {
      os << "\\l" << opts.annotations_per_node.at(id);
    }

    os << "\"";
    os << ", fillcolor=\"" << get_color(node) << "\"";
    os << "];\n";

    if (next) {
      os << "\t" << get_gv_name(node);
      os << " -> ";
      os << get_gv_name(next);
      os << ";\n";
    }

    return STOP;
  }

  void visitRoot(const Node *root) override { root->visit(*this); }

private:
  std::string get_gv_name(const Node *node) const {
    return std::to_string(node->get_id());
  }
};
} // namespace bdd
