#include "search.h"
#include "log.h"
#include "targets/targets.h"
#include "heuristics/heuristics.h"
#include "visualizers/ss_visualizer.h"
#include "visualizers/ep_visualizer.h"

#include <iomanip>

namespace synapse {

template <class HCfg>
SearchEngine<HCfg>::SearchEngine(const bdd::BDD &_bdd, Heuristic<HCfg> _h,
                                 bool _allow_bdd_reordering,
                                 const std::unordered_set<ep_id_t> &_peek)
    : bdd(std::make_shared<bdd::BDD>(_bdd)),
      targets({
          new tofino::TofinoTarget(tofino::TNAVersion::TNA2),
          new tofino_cpu::TofinoCPUTarget(),
          new x86::x86Target(),
      }),
      h(_h), allow_bdd_reordering(_allow_bdd_reordering), peek(_peek) {}

template <class HCfg>
SearchEngine<HCfg>::SearchEngine(const bdd::BDD &_bdd, Heuristic<HCfg> _h)
    : SearchEngine(_bdd, _h, true, {}) {}

template <class HCfg> SearchEngine<HCfg>::~SearchEngine() {
  for (const Target *target : targets) {
    if (target) {
      delete target;
      target = nullptr;
    }
  }
}

struct search_it_report_t {
  int available_execution_plans;
  const EP *chosen;
  const bdd::Node *current;

  std::vector<TargetType> targets;
  std::vector<std::string> name;
  std::vector<std::vector<ep_id_t>> gen_ep_ids;

  search_it_report_t(int _available_execution_plans, const EP *_chosen,
                     const bdd::Node *_current)
      : available_execution_plans(_available_execution_plans), chosen(_chosen),
        current(_current) {}

  void save(const ModuleGenerator *modgen,
            const std::vector<const EP *> &new_eps) {
    if (new_eps.empty()) {
      return;
    }

    targets.push_back(modgen->get_target());
    name.push_back(modgen->get_name());
    gen_ep_ids.emplace_back();
    available_execution_plans += new_eps.size();

    for (const EP *next_ep : new_eps) {
      ep_id_t next_ep_id = next_ep->get_id();
      gen_ep_ids.back().push_back(next_ep_id);
    }
  }
};

static void log_search_iteration(const search_it_report_t &report) {
  TargetType platform = report.chosen->get_current_platform();
  const EPLeaf *leaf = report.chosen->get_active_leaf();
  const EPMeta &meta = report.chosen->get_meta();

  Log::dbg() << "\n";
  Log::dbg() << "=======================================================\n";

  Log::dbg() << "EP ID:      " << report.chosen->get_id() << "\n";
  Log::dbg() << "Target:     " << platform << "\n";
  Log::dbg() << "Progress:   " << std::fixed << std::setprecision(2)
             << 100 * meta.get_bdd_progress() << " %\n";
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

  if (report.targets.size() == 0) {
    Log::dbg() << "\n";
    Log::dbg() << "**DEAD END**: No module can handle this BDD node"
                  " in the current context.\n";
    Log::dbg() << "Deleting solution from search space.\n";
  }

  Log::dbg() << "=======================================================\n";
}

static void peek_search_space(const std::vector<const EP *> &eps,
                              const std::unordered_set<ep_id_t> &peek,
                              SearchSpace *search_space) {
  for (const EP *ep : eps) {
    if (peek.find(ep->get_id()) != peek.end()) {
      EPVisualizer::visualize(ep, false);
      SSVisualizer::visualize(search_space, ep, true);
    }
  }
}

template <class HCfg> search_product_t SearchEngine<HCfg>::search() {
  SearchSpace *search_space = new SearchSpace(h.get_cfg());

  h.add({new EP(bdd, targets)});

  while (!h.finished()) {
    const EP *ep = h.pop();
    search_space->activate_leaf(ep);

    size_t available = h.size();
    const bdd::Node *node = ep->get_next_node();
    search_it_report_t report(available, ep, node);

    std::vector<const EP *> new_eps;

    for (const Target *target : targets) {
      for (const ModuleGenerator *modgen : target->module_generators) {
        std::vector<const EP *> modgen_new_eps =
            modgen->generate(ep, node, allow_bdd_reordering);
        new_eps.insert(new_eps.end(), modgen_new_eps.begin(),
                       modgen_new_eps.end());
        search_space->add_to_active_leaf(node, modgen, modgen_new_eps);
        report.save(modgen, modgen_new_eps);
      }
    }

    h.add(new_eps);

    log_search_iteration(report);
    peek_search_space(new_eps, peek, search_space);

    delete ep;
  }

  Log::log() << "Random seed:     " << h.get_random_seed() << "\n";
  Log::log() << "Solutions:       " << h.get_all().size() << "\n";
  Log::log() << "Winner:          " << h.get_score(h.get()) << "\n";

  EP *winner = new EP(*h.get());

  return search_product_t(winner, search_space);
}

EXPLICIT_HEURISTIC_TEMPLATE_CLASS_INSTANTIATION(SearchEngine)

} // namespace synapse