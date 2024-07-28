#include "search.h"
#include "log.h"
#include "targets/targets.h"
#include "heuristics/heuristics.h"
#include "visualizers/ss_visualizer.h"
#include "visualizers/ep_visualizer.h"

#include <chrono>
#include <iomanip>

namespace synapse {

template <class HCfg>
SearchEngine<HCfg>::SearchEngine(const bdd::BDD *_bdd, Heuristic<HCfg> *_h,
                                 Profiler *_profiler, const targets_t &_targets,
                                 bool _allow_bdd_reordering,
                                 const std::unordered_set<ep_id_t> &_peek,
                                 bool _pause_and_show_on_backtrack)
    : bdd(new bdd::BDD(*_bdd)), h(_h), profiler(new Profiler(*_profiler)),
      targets(_targets), allow_bdd_reordering(_allow_bdd_reordering),
      peek(_peek), pause_and_show_on_backtrack(_pause_and_show_on_backtrack) {}

template <class HCfg>
SearchEngine<HCfg>::SearchEngine(const bdd::BDD *_bdd, Heuristic<HCfg> *_h,
                                 Profiler *_profiler, const targets_t &_targets)
    : SearchEngine(_bdd, _h, _profiler, _targets, true, {}, false) {}

struct search_step_report_t {
  int available_execution_plans;
  const EP *chosen;
  const bdd::Node *current;

  std::vector<TargetType> targets;
  std::vector<std::string> name;
  std::vector<std::vector<ep_id_t>> gen_ep_ids;

  search_step_report_t(int _available_execution_plans, const EP *_chosen,
                       const bdd::Node *_current)
      : available_execution_plans(_available_execution_plans), chosen(_chosen),
        current(_current) {}

  void save(const ModuleGenerator *modgen,
            const std::vector<generator_product_t> &products) {
    if (products.empty()) {
      return;
    }

    targets.push_back(modgen->get_target());
    name.push_back(modgen->get_name());
    gen_ep_ids.emplace_back();
    available_execution_plans += products.size();

    for (const generator_product_t &product : products) {
      ep_id_t next_ep_id = product.ep->get_id();
      gen_ep_ids.back().push_back(next_ep_id);
    }
  }
};

static void log_search_iteration(const search_step_report_t &report,
                                 const search_meta_t &search_meta) {
  TargetType platform = report.chosen->get_current_platform();
  const EPLeaf *leaf = report.chosen->get_active_leaf();
  const EPMeta &meta = report.chosen->get_meta();

  Log::dbg() << "\n";
  Log::dbg() << "==========================================================\n";

  Log::dbg() << "EP ID:      " << report.chosen->get_id() << "\n";
  Log::dbg() << "Target:     " << platform << "\n";
  Log::dbg() << "Available:  " << report.available_execution_plans << "\n";
  if (leaf && leaf->node) {
    const Module *module = leaf->node->get_module();
    Log::dbg() << "Leaf:       " << module->get_name() << "\n";
  }
  Log::dbg() << "Node:       " << report.current->dump(true) << "\n";

  assert(report.targets.size() == report.name.size() &&
         report.targets.size() == report.gen_ep_ids.size() &&
         "Mismatch in the number of targets");

  for (size_t i = 0; i < report.targets.size(); i++) {
    std::stringstream ep_ids;

    ep_ids << "[";
    for (size_t j = 0; j < report.gen_ep_ids[i].size(); j++) {
      if (j != 0) {
        ep_ids << ",";
      }
      ep_ids << report.gen_ep_ids[i][j];
    }
    ep_ids << "]";

    Log::dbg() << "MATCH:      " << report.targets[i] << "::" << report.name[i]
               << " -> " << report.gen_ep_ids[i].size() << " (" << ep_ids.str()
               << ") EPs\n";
  }

  Log::dbg() << "------------------------------------------\n";
  Log::dbg() << "Progress:         " << std::fixed << std::setprecision(2)
             << 100 * meta.get_bdd_progress() << " %\n";
  Log::dbg() << "Elapsed:          " << search_meta.elapsed_time << " s\n";
  Log::dbg() << "Backtracks:       " << int2hr(search_meta.backtracks) << "\n";
  Log::dbg() << "Branching factor: " << search_meta.branching_factor << "\n";
  Log::dbg() << "SS size:          " << int2hr(search_meta.ss_size) << "\n";
  Log::dbg() << "Search Steps:     " << int2hr(search_meta.steps) << "\n";

  if (report.targets.size() == 0) {
    Log::dbg() << "\n";
    Log::dbg() << "**DEAD END**: No module can handle this BDD node"
                  " in the current context.\n";
    Log::dbg() << "Deleting solution from search space.\n";
  }

  Log::dbg() << "==========================================================\n";
}

static void peek_search_space(const std::vector<const EP *> &eps,
                              const std::unordered_set<ep_id_t> &peek,
                              SearchSpace *search_space) {
  for (const EP *ep : eps) {
    if (peek.find(ep->get_id()) != peek.end()) {
      bdd::BDDVisualizer::visualize(ep->get_bdd(), false);
      EPVisualizer::visualize(ep, false);
      SSVisualizer::visualize(search_space, ep, true);
    }
  }
}

static void peek_backtrack(const EP *ep, SearchSpace *search_space,
                           bool pause_and_show_on_backtrack) {
  if (pause_and_show_on_backtrack) {
    Log::dbg() << "Backtracked to " << ep->get_id() << "\n";
    bdd::BDDVisualizer::visualize(ep->get_bdd(), false);
    // EPVisualizer::visualize(ep, false);
    SSVisualizer::visualize(search_space, ep, true);
  }
}

template <class HCfg> search_report_t SearchEngine<HCfg>::search() {
  search_meta_t meta = {
      .ss_size = 0,
      .elapsed_time = 0,
      .steps = 0,
      .backtracks = 0,
      .branching_factor = 0,
  };

  auto start_search = std::chrono::steady_clock::now();

  SearchSpace *search_space = new SearchSpace(h->get_cfg());

  h->add({new EP(bdd, targets, profiler)});

  while (!h->finished()) {
    meta.ss_size = search_space->get_size();
    meta.elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - start_search)
                            .count();
    meta.steps++;
    meta.branching_factor = meta.ss_size / (float)meta.steps;

    const EP *ep = h->pop();
    search_space->activate_leaf(ep);

    if (search_space->is_backtrack()) {
      meta.backtracks++;
      peek_backtrack(ep, search_space, pause_and_show_on_backtrack);
    }

    const bdd::Node *node = ep->get_next_node();
    search_step_report_t report(h->size(), ep, node);

    std::vector<const EP *> new_eps;

    for (const Target *target : targets) {
      for (const ModuleGenerator *modgen : target->module_generators) {
        std::vector<generator_product_t> modgen_products =
            modgen->generate(ep, node, allow_bdd_reordering);
        search_space->add_to_active_leaf(ep, node, modgen, modgen_products);
        report.save(modgen, modgen_products);

        for (const generator_product_t &product : modgen_products) {
          new_eps.push_back(product.ep);
        }
      }
    }

    h->add(new_eps);

    log_search_iteration(report, meta);
    peek_search_space(new_eps, peek, search_space);

    delete ep;
  }

  EP *winner = new EP(*h->get());

  const search_config_t config = {
      .heuristic = h->get_cfg()->name,
      .random_seed = h->get_random_seed(),
  };

  const search_solution_t solution = {
      .ep = winner,
      .search_space = search_space,
      .score = h->get_score(winner),
      .throughput_estimation =
          SearchSpace::build_meta_throughput_estimate(winner),
      .throughput_speculation =
          SearchSpace::build_meta_throughput_speculation(winner),
  };

  search_report_t report = {
      .config = config,
      .solution = solution,
      .meta = meta,
  };

  return report;
}

EXPLICIT_HEURISTIC_TEMPLATE_CLASS_INSTANTIATION(SearchEngine)

} // namespace synapse