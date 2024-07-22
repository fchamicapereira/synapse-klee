#include "search_space.h"

#include "bdd-visualizer.h"

namespace synapse {

static ss_node_id_t node_id_counter = 0;

void SearchSpace::activate_leaf(const EP *ep) {
  ep_id_t ep_id = ep->get_id();

  if (!root) {
    ss_node_id_t id = node_id_counter++;
    Score score = hcfg->get_score(ep);
    TargetType target = ep->get_current_platform();
    root = new SSNode(id, ep_id, score, target);
    active_leaf = root;
    return;
  }

  auto ss_node_matcher = [ep_id](const SSNode *node) {
    return node->ep_id == ep_id;
  };

  auto found_it = std::find_if(leaves.begin(), leaves.end(), ss_node_matcher);
  assert(found_it != leaves.end() && "Leaf not found");

  active_leaf = *found_it;
  leaves.erase(found_it);

  backtrack = (last_eps.find(ep_id) == last_eps.end());
  last_eps.clear();
}

static std::string get_bdd_node_description(const bdd::Node *node) {
  std::stringstream description;

  description << node->get_id();
  description << ": ";

  switch (node->get_type()) {
  case bdd::NodeType::CALL: {
    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    description << call_node->get_call().function_name;
  } break;
  case bdd::NodeType::BRANCH: {
    const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);
    klee::ref<klee::Expr> condition = branch_node->get_condition();
    description << "if (";
    description << kutil::pretty_print_expr(condition);
    description << ")";
  } break;
  case bdd::NodeType::ROUTE: {
    const bdd::Route *route = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route->get_operation();

    switch (op) {
    case bdd::RouteOperation::BCAST: {
      description << "broadcast()";
    } break;
    case bdd::RouteOperation::DROP: {
      description << "drop()";
    } break;
    case bdd::RouteOperation::FWD: {
      description << "forward(";
      description << route->get_dst_device();
      description << ")";
    } break;
    }
  } break;
  }

  std::string node_str = description.str();

  constexpr int MAX_STR_SIZE = 250;
  if (node_str.size() > MAX_STR_SIZE) {
    node_str = node_str.substr(0, MAX_STR_SIZE);
    node_str += " [...]";
  }

  Graphviz::sanitize_html_label(node_str);

  return node_str;
}

static std::string build_meta_throughput_estimate(const EP *ep) {
  std::stringstream ss;

  ss << "Throughput: ";

  const Context &ctx = ep->get_ctx();
  const Profiler *profiler = ctx.get_profiler();
  int avg_pkt_size = profiler->get_avg_pkt_bytes();

  uint64_t estimate_pps = ep->estimate_throughput_pps();
  uint64_t estimate_bps = estimate_pps * avg_pkt_size * 8;

  ss << throughput2str(estimate_bps, "bps", true);

  ss << " (";
  ss << throughput2str(estimate_pps, "pps", true);
  ss << ")";

  return ss.str();
}

static std::string build_meta_throughput_speculation(const EP *ep) {
  std::stringstream ss;

  ss << "Speculation: ";

  const Context &ctx = ep->get_ctx();
  const Profiler *profiler = ctx.get_profiler();
  int avg_pkt_size = profiler->get_avg_pkt_bytes();

  uint64_t speculation_pps = ep->speculate_throughput_pps();
  uint64_t speculation_bps = speculation_pps * avg_pkt_size * 8;

  ss << throughput2str(speculation_bps, "bps", true);

  ss << " (";
  ss << throughput2str(speculation_pps, "pps", true);
  ss << ")";

  return ss.str();
}

void SearchSpace::add_to_active_leaf(
    const EP *ep, const bdd::Node *node, const ModuleGenerator *modgen,
    const std::vector<generator_product_t> &products) {
  assert(active_leaf && "Active leaf not set");

  for (const generator_product_t &product : products) {
    ss_node_id_t id = node_id_counter++;
    ep_id_t ep_id = product.ep->get_id();
    Score score = hcfg->get_score(product.ep);
    TargetType target = modgen->get_target();
    const bdd::Node *next = product.ep->get_next_node();

    module_data_t module_data = {
        .type = modgen->get_type(),
        .name = modgen->get_name(),
        .description = product.description,
        .hit_rate = ep->get_active_leaf_hit_rate(),
    };

    bdd_node_data_t bdd_node_data = {
        .id = node->get_id(),
        .description = get_bdd_node_description(node),
    };

    std::optional<bdd_node_data_t> next_bdd_node_data;
    if (next) {
      next_bdd_node_data = {
          .id = next->get_id(),
          .description = get_bdd_node_description(next),
      };
    }

    std::vector<std::string> metadata = {
        build_meta_throughput_estimate(product.ep),
        build_meta_throughput_speculation(product.ep),
    };

    SSNode *new_node = new SSNode(id, ep_id, score, target, module_data,
                                  bdd_node_data, next_bdd_node_data, metadata);

    active_leaf->children.push_back(new_node);
    leaves.push_back(new_node);

    size++;
    last_eps.insert(ep_id);
  }
}

SSNode *SearchSpace::get_root() const { return root; }
size_t SearchSpace::get_size() const { return size; }
const HeuristicCfg *SearchSpace::get_hcfg() const { return hcfg; }
bool SearchSpace::is_backtrack() const { return backtrack; }

} // namespace synapse