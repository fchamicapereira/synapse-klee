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
                                 Profiler *_profiler,
                                 bool _allow_bdd_reordering,
                                 const std::unordered_set<ep_id_t> &_peek)
    : bdd(new bdd::BDD(*_bdd)),
      targets({
          new tofino::TofinoTarget(tofino::TNAVersion::TNA2, _profiler),
          new tofino_cpu::TofinoCPUTarget(),
          new x86::x86Target(),
      }),
      h(_h), profiler(new Profiler(*_profiler)),
      allow_bdd_reordering(_allow_bdd_reordering), peek(_peek) {}

template <class HCfg>
SearchEngine<HCfg>::SearchEngine(const bdd::BDD *_bdd, Heuristic<HCfg> *_h,
                                 Profiler *_profiler)
    : SearchEngine(_bdd, _h, _profiler, true, {}) {}

template <class HCfg> SearchEngine<HCfg>::~SearchEngine() {
  for (const Target *target : targets) {
    if (target) {
      delete target;
      target = nullptr;
    }
  }
}

search_report_t::search_report_t(const EP *_ep,
                                 const SearchSpace *_search_space,
                                 const std::string &_heuristic_name,
                                 unsigned _random_seed, size_t _ss_size,
                                 Score _winner_score, double _search_time)
    : ep(_ep), search_space(_search_space), heuristic_name(_heuristic_name),
      random_seed(_random_seed), ss_size(_ss_size), winner_score(_winner_score),
      search_time(_search_time) {}

search_report_t::search_report_t(search_report_t &&other)
    : ep(std::move(other.ep)), search_space(std::move(other.search_space)),
      heuristic_name(std::move(other.heuristic_name)),
      random_seed(std::move(other.random_seed)),
      ss_size(std::move(other.ss_size)),
      winner_score(std::move(other.winner_score)),
      search_time(std::move(other.search_time)) {}

search_report_t::~search_report_t() {
  if (ep) {
    delete ep;
    ep = nullptr;
  }

  if (search_space) {
    delete search_space;
    search_space = nullptr;
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

static void log_search_iteration(const search_it_report_t &report) {
  TargetType platform = report.chosen->get_current_platform();
  const EPLeaf *leaf = report.chosen->get_active_leaf();
  const EPMeta &meta = report.chosen->get_meta();

  Log::dbg() << "\n";
  Log::dbg() << "==========================================================\n";

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

template <class HCfg> search_report_t SearchEngine<HCfg>::search() {
  auto start_search = std::chrono::steady_clock::now();

  SearchSpace *search_space = new SearchSpace(h->get_cfg());

  h->add({new EP(bdd, targets, profiler)});

  while (!h->finished()) {
    const EP *ep = h->pop();
    search_space->activate_leaf(ep);

    size_t available = h->size();
    const bdd::Node *node = ep->get_next_node();
    search_it_report_t report(available, ep, node);

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

    log_search_iteration(report);
    peek_search_space(new_eps, peek, search_space);

    delete ep;
  }

  auto end_search = std::chrono::steady_clock::now();
  auto search_dt = std::chrono::duration_cast<std::chrono::seconds>(
                       end_search - start_search)
                       .count();

  EP *winner = new EP(*h->get());

  const std::string heuristic_name = h->get_cfg()->name;
  const unsigned random_seed = h->get_random_seed();
  const size_t ss_size = search_space->get_size();
  const Score winner_score = h->get_score(winner);
  const double search_time = search_dt;

  return search_report_t(winner, search_space, heuristic_name, random_seed,
                         ss_size, winner_score, search_time);
}

EXPLICIT_HEURISTIC_TEMPLATE_CLASS_INSTANTIATION(SearchEngine)

} // namespace synapse