#include "search.h"
#include "log.h"
#include "targets/module_generator.h"
#include "visualizers/ss_visualizer.h"
#include "heuristics/heuristics.h"

#include <iomanip>

namespace synapse {

template <class HCfg>
SearchEngine<HCfg>::SearchEngine(const bdd::BDD &_bdd, Heuristic<HCfg> _h,
                                 bool _bdd_reordering,
                                 const bdd::nodes_t &_nodes_to_peek)
    : bdd(std::make_shared<bdd::BDD>(_bdd)), h(_h), search_space(_h.get_cfg()),
      bdd_reordering(_bdd_reordering), nodes_to_peek(_nodes_to_peek) {
  // targets.push_back(new Tofino::TofinoTarget());
  // targets.push_back(new TofinoCPU::TofinoCPUTarget());
  targets.push_back(new x86::x86Target());
}

template <class HCfg>
SearchEngine<HCfg>::SearchEngine(const bdd::BDD &_bdd, Heuristic<HCfg> _h)
    : SearchEngine(_bdd, _h, true, {}) {}

struct search_it_report_t {
  int available_execution_plans;
  const EP &chosen;
  const bdd::Node *current;

  std::vector<TargetType> targets;
  std::vector<std::string> name;
  std::vector<size_t> generated_contexts;
  std::vector<std::vector<ep_id_t>> generated_exec_plans_ids;

  search_it_report_t(int _available_execution_plans, const EP &_chosen,
                     const bdd::Node *_current)
      : available_execution_plans(_available_execution_plans), chosen(_chosen),
        current(_current) {}
};

template <class HCfg> SearchEngine<HCfg>::~SearchEngine() {
  for (const Target *target : targets) {
    if (target) {
      delete target;
      target = nullptr;
    }
  }
}

static void log_search_iteration(const search_it_report_t &report) {
  TargetType platform = report.chosen.get_current_platform();
  const EPLeaf *leaf = report.chosen.get_active_leaf();
  const EPMeta &meta = report.chosen.get_meta();

  Log::dbg() << "\n";
  Log::dbg() << "=======================================================\n";

  Log::dbg() << "Available:      " << report.available_execution_plans << "\n";
  Log::dbg() << "Execution Plan: " << report.chosen.get_id() << "\n";
  Log::dbg() << "BDD progress:   " << std::fixed << std::setprecision(2)
             << 100 * meta.get_bdd_progress() << " %\n";
  Log::dbg() << "Current target: " << platform << "\n";
  if (leaf && leaf->node) {
    const Module *module = leaf->node->get_module();
    Log::dbg() << "Leaf:           " << module->get_name() << "\n";
  }
  Log::dbg() << "Node:           " << report.current->dump(true) << "\n";

  assert(report.targets.size() == report.name.size() &&
         report.targets.size() == report.generated_contexts.size() &&
         report.targets.size() == report.generated_exec_plans_ids.size() &&
         "Mismatch in the number of targets and generated contexts");

  if (report.targets.size()) {
    for (size_t i = 0; i < report.targets.size(); i++) {
      std::stringstream exec_plans_ids;
      exec_plans_ids << "[";
      for (size_t j = 0; j < report.generated_exec_plans_ids[i].size(); j++) {
        if (j != 0) {
          exec_plans_ids << ",";
        }
        exec_plans_ids << report.generated_exec_plans_ids[i][j];
      }
      exec_plans_ids << "]";

      Log::dbg() << "MATCH:          " << report.targets[i]
                 << "::" << report.name[i] << " -> " << exec_plans_ids.str()
                 << " (" << report.generated_contexts[i] << ") exec plans "
                 << "\n";
    }
  } else {
    Log::dbg() << "\n";
    Log::dbg() << "**DEAD END**: No module can handle this BDD node"
                  " in the current context.\n";
    Log::dbg() << "Deleting solution from search space.\n";
  }

  Log::dbg() << "=======================================================\n";
}

template <class HCfg> EP SearchEngine<HCfg>::search() {
  EP initial_ep(bdd, targets);
  h.add({initial_ep});

  while (!h.finished()) {
    EP ep = h.pop();
    search_space.activate_leaf(ep);

    size_t available = h.size();
    const bdd::Node *node = ep.get_next_node();
    search_it_report_t report(available, ep, node);

    for (const Target *target : targets) {
      for (const ModuleGenerator *modgen : target->module_generators) {
        modgen_report_t result = modgen->generate(ep, node);

        if (!result.next.size())
          continue;

        report.targets.push_back(result.module->get_target());
        report.name.push_back(result.module->get_name());
        report.generated_contexts.push_back(result.next.size());
        report.generated_exec_plans_ids.emplace_back();

        for (const EP &next_ep : result.next) {
          ep_id_t next_ep_id = next_ep.get_id();
          report.generated_exec_plans_ids.back().push_back(next_ep_id);
        }

        h.add(result.next);
        search_space.add_to_active_leaf(node, result.module, result.next);
      }
    }

    log_search_iteration(report);

    if (nodes_to_peek.find(node->get_id()) != nodes_to_peek.end()) {
      SSVisualizer::visualize(search_space, true);
    }
  }

  Log::log() << "Solutions:      " << h.get_all().size() << "\n";
  Log::log() << "Winner:         " << h.get_score(h.get()) << "\n";

  return h.get();
}

EXPLICIT_HEURISTIC_TEMPLATE_CLASS_INSTANTIATION(SearchEngine)

} // namespace synapse