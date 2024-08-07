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

SSVisualizer::SSVisualizer(const EP *_highlight) {
  const std::set<ep_id_t> &ancestors = _highlight->get_ancestors();
  highlight.insert(ancestors.begin(), ancestors.end());
  highlight.insert(_highlight->get_id());
}

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

static std::string bold(const std::string &str) { return "<b>" + str + "</b>"; }

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

  ss << " bgcolor=\"" << target_color << "\"";

  if (should_highlight(ssnode, highlight)) {
    ss << " border=\"4\"";
    ss << " color=\"" << selected_color << "\"";
  } else {
    ss << " border=\"2\"";
    ss << " color=\"black\"";
  }
  ss << ">\n";

  // First row

  indent(3);
  ss << "<tr>\n";

  indent(4);
  ss << "<td ";
  // ss << "bgcolor=\"" << target_color << "\"";
  ss << ">";
  ss << bold("EP: ") << ssnode->ep_id;
  ss << "</td>\n";

  indent(4);
  ss << "<td ";
  // ss << "bgcolor=\"" << target_color << "\"";
  ss << ">";
  if (ssnode->module_data) {
    ss << bold("HR: ") << ssnode->module_data->hit_rate;
  } else {
    ss << bold("HR: ") << "None";
  }
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  // Second row

  indent(3);
  ss << "<tr>\n";

  indent(4);
  ss << "<td";
  // ss << " bgcolor=\"" << target_color << "\"";
  ss << " colspan=\"2\"";
  ss << ">";

  ss << bold("Score: ");
  ss << stringify_score(ssnode->score);
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  // Third row

  indent(3);
  ss << "<tr>\n";

  indent(4);
  ss << "<td";
  // ss << " bgcolor=\"" << target_color << "\"";
  ss << " colspan=\"2\"";
  ss << ">";
  ss << bold("Module: ");
  if (ssnode->module_data) {
    ss << ssnode->module_data->name;
    ss << " (";
    ss << ssnode->module_data->description;
    ss << ")";
  } else {
    ss << "ROOT";
  }
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  // Forth row

  indent(3);
  ss << "<tr>\n";

  indent(4);
  ss << "<td ";
  // ss << " bgcolor=\"" << target_color << "\"";
  ss << " colspan=\"2\"";
  ss << ">";
  if (ssnode->bdd_node_data) {
    ss << bold("Processed: ");
    ss << ssnode->bdd_node_data->description;
  }
  ss << "</td>\n";

  indent(3);
  ss << "</tr>\n";

  // Fifth row

  if (ssnode->next_bdd_node_data) {
    indent(3);
    ss << "<tr>\n";

    indent(4);
    ss << "<td ";
    // ss << " bgcolor=\"" << target_color << "\"";
    ss << " colspan=\"2\"";
    ss << ">";
    ss << bold("Next: ");
    ss << ssnode->next_bdd_node_data->description;
    ss << "</td>\n";

    indent(3);
    ss << "</tr>\n";
  }

  // Metadata rows

  for (const auto &[name, value] : ssnode->metadata) {
    indent(3);
    ss << "<tr>\n";

    indent(4);

    ss << "<td ";
    // ss << " bgcolor=\"" << target_color << "\"";
    ss << " colspan=\"2\"";
    ss << ">";
    ss << bold(name + ": ");
    ss << value;
    ss << "</td>\n";

    indent(3);
    ss << "</tr>\n";
  }

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
  assert(search_space);
  Log::log() << "Visualizing SS";
  Log::log() << " file=" << fname;
  if (ep)
    Log::log() << " highlight=" << ep->get_id();
  Log::log() << "\n";
}

void SSVisualizer::visualize(const SearchSpace *search_space, bool interrupt) {
  assert(search_space);
  SSVisualizer visualizer;
  visualizer.visit(search_space);
  log_visualization(search_space, visualizer.fpath);
  visualizer.show(interrupt);
}

void SSVisualizer::visualize(const SearchSpace *search_space,
                             const EP *highlight, bool interrupt) {
  assert(search_space);
  SSVisualizer visualizer(highlight);
  visualizer.visit(search_space);
  log_visualization(search_space, visualizer.fpath, highlight);
  visualizer.show(interrupt);
}

} // namespace synapse