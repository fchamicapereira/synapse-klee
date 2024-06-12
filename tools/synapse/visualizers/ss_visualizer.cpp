#include "ss_visualizer.h"

#include <unordered_map>

namespace synapse {

static std::unordered_map<TargetType, std::string> node_colors = {
    {TargetType::Tofino, "cornflowerblue"},
    {TargetType::TofinoCPU, "firebrick2"},
    {TargetType::x86, "orange"},
};

static std::string selected_color = "green";

SSVisualizer::SSVisualizer() {}

SSVisualizer::SSVisualizer(const EP *_highlight)
    : highlight(_highlight->get_ancestors()) {}

static std::string stringify_score(const Score &score) {
  std::stringstream score_builder;
  score_builder << score;

  std::string score_str = score_builder.str();
  Graphviz::sanitize_html_label(score_str);

  return score_str;
}

static bool should_highlight(const SSNode *ssnode,
                             const std::set<ep_id_t> &highlight) {
  return highlight.find(ssnode->ep_id) != highlight.end();
}

static void visit_definitions(std::stringstream &ss,
                              const SearchSpace *search_space,
                              const SSNode *ssnode,
                              const std::set<ep_id_t> &highlight) {
  const std::string &target_color = node_colors.at(ssnode->target);

  auto indent = [&ss](int lvl) { ss << std::string(lvl, '\t'); };

  indent(1);
  ss << ssnode->node_id;
  ss << " [label=<\n";

  indent(2);
  ss << "<table";

  if (should_highlight(ssnode, highlight)) {
    ss << " border=\"4\"";
    ss << " bgcolor=\"blue\"";
    ss << " color=\"" << selected_color << "\"";
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

  ss << stringify_score(ssnode->score);
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  // Second row

  indent(3);
  ss << "<tr>\n";

  if (ssnode->module_data) {
    indent(4);
    ss << "<td ";
    ss << "bgcolor=\"" << target_color << "\"";
    ss << ">";
    ss << "Fraction: " << ssnode->module_data->hit_rate;
    ss << "</td>\n";
  }

  indent(4);
  ss << "<td";
  ss << " bgcolor=\"" << target_color << "\"";
  ss << " colspan=\"2\"";
  ss << ">";
  if (ssnode->module_data) {
    ss << ssnode->module_data->name;
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
  if (ssnode->bdd_node_data) {
    ss << ssnode->bdd_node_data->description;
  }
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  indent(2);
  ss << "</table>\n";

  indent(1);
  ss << ">]\n\n";

  for (const SSNode *next : ssnode->children) {
    visit_definitions(ss, search_space, next, highlight);
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

void SSVisualizer::visit(const SearchSpace *search_space) {
  ss << "digraph SearchSpace {\n";

  ss << "\tlayout=\"dot\";\n";
  // ss << "\tlayout=\"twopi\";\n";
  // ss << "\tlayout=\"sfdp\";\n";
  // ss << "\tgraph [splines=true overlap=false];\n";
  ss << "\tnode [shape=none];\n";

  const SSNode *root = search_space->get_root();

  if (root) {
    visit_definitions(ss, search_space, root, highlight);
    visit_links(ss, root);
  }

  ss << "}";
  ss.flush();
}

static void log_visualization(const SearchSpace *search_space,
                              const std::string &fname,
                              const EP *ep = nullptr) {
  std::cerr << "Visualizing SS";
  std::cerr << " file=" << fname;
  if (ep)
    std::cerr << " highlight=" << ep->get_id();
  std::cerr << "\n";
}

void SSVisualizer::visualize(const SearchSpace *search_space, bool interrupt) {
  SSVisualizer visualizer;
  visualizer.visit(search_space);
  log_visualization(search_space, visualizer.fpath);
  visualizer.show(interrupt);
}

void SSVisualizer::visualize(const SearchSpace *search_space,
                             const EP *highlight, bool interrupt) {
  SSVisualizer visualizer(highlight);
  visualizer.visit(search_space);
  log_visualization(search_space, visualizer.fpath, highlight);
  visualizer.show(interrupt);
}

} // namespace synapse