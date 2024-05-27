#include "ss_visualizer.h"

#include <unordered_map>

namespace synapse {

static std::unordered_map<TargetType, std::string> node_colors = {
    {TargetType::Tofino, "cornflowerblue"},
    {TargetType::TofinoCPU, "firebrick2"},
    {TargetType::x86, "cadetblue1"},
};

SSVisualizer::SSVisualizer() {}

static std::string stringify_score(const Score &score) {
  std::stringstream score_builder;
  score_builder << score;

  std::string score_str = score_builder.str();
  Graphviz::sanitize_html_label(score_str);

  return score_str;
}

static std::string stringify_bdd_node(const bdd::Node *node) {
  std::stringstream node_builder;

  node_builder << node->get_id();
  node_builder << ": ";

  switch (node->get_type()) {
  case bdd::NodeType::CALL: {
    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    node_builder << call_node->get_call().function_name;
  } break;
  case bdd::NodeType::BRANCH: {
    const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);
    klee::ref<klee::Expr> condition = branch_node->get_condition();
    node_builder << "if (";
    node_builder << kutil::pretty_print_expr(condition);
    node_builder << ")";
  } break;
  case bdd::NodeType::ROUTE: {
    const bdd::Route *route = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route->get_operation();

    switch (op) {
    case bdd::RouteOperation::BCAST: {
      node_builder << "broadcast()";
    } break;
    case bdd::RouteOperation::DROP: {
      node_builder << "drop()";
    } break;
    case bdd::RouteOperation::FWD: {
      node_builder << "forward(";
      node_builder << route->get_dst_device();
      node_builder << ")";
    } break;
    }
  } break;
  }

  std::string node_str = node_builder.str();

  constexpr int MAX_STR_SIZE = 250;
  if (node_str.size() > MAX_STR_SIZE) {
    node_str = node_str.substr(0, MAX_STR_SIZE);
    node_str += " [...]";
  }

  Graphviz::sanitize_html_label(node_str);

  return node_str;
}

static void visit_definitions(std::stringstream &ss,
                              const SearchSpace &search_space,
                              const SSNode *ssnode) {
  ss_node_id_t ss_node_id = ssnode->node_id;
  const std::vector<SSNode *> &next_nodes = ssnode->children;
  const Module *module = ssnode->module;
  const bdd::Node *node = ssnode->node;
  const Score &score = ssnode->score;
  TargetType target = module->get_target();
  std::string target_color = node_colors.at(target);

  auto indent = [&ss](int lvl) { ss << std::string(lvl, '\t'); };

  indent(1);
  ss << ss_node_id;
  ss << " [label=<\n";

  indent(2);
  ss << "<table";

  if (search_space.was_explored(ss_node_id)) {
    ss << " border=\"4\"";
    ss << " bgcolor=\"blue\"";
    ss << " color=\"green\"";
  }
  ss << ">\n";

  // First row

  indent(3);
  ss << "<tr>\n";

  indent(4);
  ss << "<td ";
  ss << "bgcolor=\"" << target_color << "\"";
  ss << ">";
  ss << "EP: " << ssnode->ep_id;
  ss << "</td>\n";

  indent(4);
  ss << "<td ";
  ss << "bgcolor=\"" << target_color << "\"";
  ss << ">";

  ss << stringify_score(score);
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  // Second row

  indent(3);
  ss << "<tr>\n";

  indent(4);
  ss << "<td";
  ss << " bgcolor=\"" << target_color << "\"";
  ss << " colspan=\"2\"";
  ss << ">";
  if (module) {
    ss << module->get_name();
  } else {
    ss << "ROOT";
  }
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  // Third row

  indent(3);
  ss << "<tr>\n";

  indent(4);
  ss << "<td ";
  ss << " bgcolor=\"" << target_color << "\"";
  ss << " colspan=\"2\"";
  ss << ">";
  if (node) {
    ss << stringify_bdd_node(node);
  }
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  indent(2);
  ss << "</table>\n";

  indent(1);
  ss << ">]\n\n";

  for (const SSNode *next : next_nodes) {
    visit_definitions(ss, search_space, next);
  }
}

static void visit_links(std::stringstream &ss, const SSNode *ssnode) {
  ss_node_id_t node_id = ssnode->node_id;
  const std::vector<SSNode *> &children = ssnode->children;

  for (const SSNode *child : children) {
    ss << node_id;
    ss << " -> ";
    ss << child->node_id;
    ss << ";\n";
  }

  for (const SSNode *child : children) {
    visit_links(ss, child);
  }
}

void SSVisualizer::visit(const SearchSpace &search_space) {
  ss << "digraph SearchSpace {\n";

  ss << "\tlayout=\"dot\";\n";
  ss << "\tnode [shape=none];\n";

  const SSNode *root = search_space.get_root();

  visit_definitions(ss, search_space, root);
  visit_links(ss, root);

  ss << "}";
  ss.flush();

  bdd_fpaths.clear();
}

void SSVisualizer::visualize(const SearchSpace &search_space, bool interrupt) {
  SSVisualizer visualizer;
  visualizer.visit(search_space);
  visualizer.show(interrupt);
}

} // namespace synapse