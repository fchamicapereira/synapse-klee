#include "bdd_visualizer.h"

namespace bdd {

static const char *COLOR_PROCESSED = "gray";
static const char *COLOR_NEXT = "cyan";

static const char *COLOR_CALL = "cornflowerblue";
static const char *COLOR_BRANCH = "yellow";
static const char *COLOR_FORWARD = "chartreuse2";
static const char *COLOR_DROP = "brown1";
static const char *COLOR_BROADCAST = "purple";

BDDVisualizer::BDDVisualizer(const bdd_visualizer_opts_t &_opts)
    : Graphviz(_opts.fname), opts(_opts) {}

BDDVisualizer::BDDVisualizer() : Graphviz() {}

std::string BDDVisualizer::get_color(const Node *node) const {
  node_id_t id = node->get_id();

  if (opts.colors_per_node.find(id) != opts.colors_per_node.end()) {
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
  case NodeType::CALL: {
    color = COLOR_CALL;
  } break;
  case NodeType::BRANCH: {
    color = COLOR_BRANCH;
  } break;
  case NodeType::ROUTE: {
    const Route *route = static_cast<const Route *>(node);
    RouteOperation operation = route->get_operation();
    switch (operation) {
    case RouteOperation::FWD:
      color = COLOR_FORWARD;
      break;
    case RouteOperation::DROP:
      color = COLOR_DROP;
      break;
    case RouteOperation::BCAST:
      color = COLOR_BROADCAST;
      break;
    }
  } break;
  }

  return color;
}

static void log_visualization(const BDD &bdd, const std::string &fname) {
  std::cerr << "Visualizing BDD";
  std::cerr << " id=" << bdd.get_id();
  std::cerr << " hash=" << bdd.hash();
  std::cerr << " file=" << fname;
  std::cerr << "\n";
}

void BDDVisualizer::visualize(const BDD &bdd, bool interrupt,
                              bdd_visualizer_opts_t opts) {
  BDDVisualizer visualizer(opts);
  visualizer.visit(bdd);
  log_visualization(bdd, visualizer.fpath);
  visualizer.show(interrupt);
}

void BDDVisualizer::visit(const BDD &bdd) {
  const Node *root = bdd.get_root();

  ss << "digraph mygraph {\n";
  ss << "\tnode [shape=box, style=\"rounded,filled\", border=0];\n";
  visitRoot(root);
  ss << "}";
}

BDDVisitorAction BDDVisualizer::visitBranch(const Branch *node) {
  const Node *on_true = node->get_on_true();
  const Node *on_false = node->get_on_false();

  klee::ref<klee::Expr> condition = node->get_condition();

  if (on_true)
    on_true->visit(*this);
  if (on_false)
    on_false->visit(*this);

  ss << "\t" << get_gv_name(node);
  ss << " [shape=Mdiamond, label=\"";

  ss << node->get_id() << ":";
  ss << kutil::pretty_print_expr(condition);

  if (opts.annotations_per_node.find(node->get_id()) !=
      opts.annotations_per_node.end()) {
    ss << "\\n" << opts.annotations_per_node.at(node->get_id());
  }

  ss << "\"";

  ss << ", fillcolor=\"" << get_color(node) << "\"";
  ss << "];\n";

  if (on_true) {
    ss << "\t" << get_gv_name(node);
    ss << " -> ";
    ss << get_gv_name(on_true);
    ss << " [label=\"True\"];\n";
  }

  if (on_false) {
    ss << "\t" << get_gv_name(node);
    ss << " -> ";
    ss << get_gv_name(on_false);
    ss << " [label=\"False\"];\n";
  }

  return BDDVisitorAction::STOP;
}

BDDVisitorAction BDDVisualizer::visitCall(const Call *node) {
  const call_t &call = node->get_call();
  node_id_t id = node->get_id();
  const Node *next = node->get_next();

  if (next) {
    next->visit(*this);
  }

  ss << "\t" << get_gv_name(node);
  ss << " [label=\"";
  ss << id << ":";
  ss << call.function_name;
  ss << "(";

  size_t i = 0;
  for (const std::pair<std::string, arg_t> &pair : call.args) {
    if (call.args.size() > 1) {
      ss << "\\l";
      ss << std::string(2, ' ');
    }

    ss << pair.first << ":";
    const arg_t &arg = pair.second;

    if (arg.fn_ptr_name.first) {
      ss << arg.fn_ptr_name.second;
    } else {
      ss << kutil::pretty_print_expr(arg.expr, false);

      if (!arg.in.isNull() || !arg.out.isNull()) {
        ss << "[";

        if (!arg.in.isNull()) {
          ss << kutil::pretty_print_expr(arg.in, false);
        }

        if (!arg.out.isNull() &&
            (arg.in.isNull() ||
             !kutil::solver_toolbox.are_exprs_always_equal(arg.in, arg.out))) {
          ss << " -> ";
          ss << kutil::pretty_print_expr(arg.out, false);
        }

        ss << "]";
      }
    }

    if (i != call.args.size() - 1) {
      ss << ",";
    }

    i++;
  }

  ss << ")";

  if (!call.ret.isNull()) {
    ss << " -> " << kutil::pretty_print_expr(call.ret);
  }

  const symbols_t &symbols = node->get_locally_generated_symbols();
  if (symbols.size()) {
    ss << "\\l=>{";
    bool first = true;
    for (const symbol_t &s : symbols) {
      if (!first) {
        ss << ",";
      } else {
        first = false;
      }
      ss << s.array->name;
    }
    ss << "}";
  }

  if (opts.annotations_per_node.find(node->get_id()) !=
      opts.annotations_per_node.end()) {
    ss << "\\l" << opts.annotations_per_node.at(node->get_id());
  }

  ss << "\"";

  ss << ", fillcolor=\"" << get_color(node) << "\"";
  ss << "];\n";

  if (next) {
    ss << "\t" << get_gv_name(node);
    ss << " -> ";
    ss << get_gv_name(next);
    ss << ";\n";
  }

  return BDDVisitorAction::STOP;
}

BDDVisitorAction BDDVisualizer::visitRoute(const Route *node) {
  node_id_t id = node->get_id();
  int dst_device = node->get_dst_device();
  RouteOperation operation = node->get_operation();
  const Node *next = node->get_next();

  if (next) {
    next->visit(*this);
  }

  ss << "\t" << get_gv_name(node);
  ss << " [label=\"";
  ss << id << ":";

  switch (operation) {
  case RouteOperation::FWD: {
    ss << "fwd(" << dst_device << ")";
    break;
  }
  case RouteOperation::DROP: {
    ss << "drop()";
    break;
  }
  case RouteOperation::BCAST: {
    ss << "bcast()";
    break;
  }
  }

  if (opts.annotations_per_node.find(id) != opts.annotations_per_node.end()) {
    ss << "\\l" << opts.annotations_per_node.at(id);
  }

  ss << "\"";
  ss << ", fillcolor=\"" << get_color(node) << "\"";
  ss << "];\n";

  if (next) {
    ss << "\t" << get_gv_name(node);
    ss << " -> ";
    ss << get_gv_name(next);
    ss << ";\n";
  }

  return BDDVisitorAction::STOP;
}

void BDDVisualizer::visitRoot(const Node *root) { root->visit(*this); }

} // namespace bdd