#include "klee/ExprBuilder.h"
#include "klee/perf-contracts.h"
#include "klee/util/ArrayCache.h"
#include "klee/util/ExprSMTLIBPrinter.h"
#include "klee/util/ExprVisitor.h"
#include "llvm/Support/CommandLine.h"

#include <klee/Constraints.h>
#include <klee/Solver.h>

#include <algorithm>
#include <chrono>
#include <dlfcn.h>
#include <expr/Parser.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <stack>
#include <utility>
#include <vector>
#include <random>

#include "call-paths-to-bdd.h"
#include "load-call-paths.h"

#include "log.h"
#include "visualizers/ep_visualizer.h"
#include "visualizers/ss_visualizer.h"
#include "visualizers/profiler_visualizer.h"
#include "heuristics/heuristics.h"
#include "search.h"
#include "synthesizers/synthesizers.h"

using namespace synapse;

namespace {
llvm::cl::OptionCategory SyNAPSE("SyNAPSE specific options");

llvm::cl::opt<std::string> InputBDDFile("in", llvm::cl::desc("Input BDD."),
                                        llvm::cl::Required,
                                        llvm::cl::cat(SyNAPSE));

llvm::cl::opt<std::string>
    Out("out", llvm::cl::desc("Output directory for every generated file."),
        llvm::cl::Required, llvm::cl::cat(SyNAPSE));

llvm::cl::opt<int> Seed("seed", llvm::cl::desc("Random seed."),
                        llvm::cl::ValueRequired, llvm::cl::Optional,
                        llvm::cl::init(-1), llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> BDDNoReorder("no-reorder",
                                 llvm::cl::desc("Deactivate BDD reordering."),
                                 llvm::cl::ValueDisallowed,
                                 llvm::cl::init(false), llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowEP("ep", llvm::cl::desc("Show winner Execution Plan."),
                           llvm::cl::ValueDisallowed, llvm::cl::init(false),
                           llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowSS("ss",
                           llvm::cl::desc("Show the entire search space."),
                           llvm::cl::ValueDisallowed, llvm::cl::init(false),
                           llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowBDD("bdd", llvm::cl::desc("Show the BDD's solution."),
                            llvm::cl::ValueDisallowed, llvm::cl::init(false),
                            llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowBacktrack("backtrack",
                                  llvm::cl::desc("Pause on backtrack."),
                                  llvm::cl::ValueDisallowed,
                                  llvm::cl::init(false),
                                  llvm::cl::cat(SyNAPSE));

llvm::cl::list<int>
    Peek("peek", llvm::cl::desc("Peek search space at these Execution Plans."),
         llvm::cl::Positional, llvm::cl::ZeroOrMore, llvm::cl::cat(SyNAPSE));

llvm::cl::opt<std::string> BDDProfile("prof",
                                      llvm::cl::desc("BDD profile JSON."),
                                      llvm::cl::ValueRequired,
                                      llvm::cl::Optional,
                                      llvm::cl::cat(SyNAPSE));

enum class HeuristicOption {
  BFS,
  DFS,
  RANDOM,
  GALLIUM,
  MAX_THROUGHPUT,
};

llvm::cl::opt<HeuristicOption> ChosenHeuristic(
    "heuristic", llvm::cl::desc("Chosen heuristic."),
    llvm::cl::values(clEnumValN(HeuristicOption::BFS, "bfs", "BFS"),
                     clEnumValN(HeuristicOption::DFS, "dfs", "DFS"),
                     clEnumValN(HeuristicOption::RANDOM, "random", "Random"),
                     clEnumValN(HeuristicOption::GALLIUM, "gallium", "Gallium"),
                     clEnumValN(HeuristicOption::MAX_THROUGHPUT,
                                "max-throughput", "Maximize throughput"),
                     clEnumValEnd),
    llvm::cl::Required, llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> Verbose("v", llvm::cl::desc("Verbose mode."),
                            llvm::cl::ValueDisallowed, llvm::cl::init(false),
                            llvm::cl::cat(SyNAPSE));
} // namespace

search_report_t search(const bdd::BDD *bdd, Profiler *profiler,
                       const targets_t &targets) {
  std::unordered_set<ep_id_t> peek;
  for (ep_id_t ep_id : Peek) {
    peek.insert(ep_id);
  }

  // A bit disgusting, but oh well...
  switch (ChosenHeuristic) {
  case HeuristicOption::BFS: {
    BFS heuristic;
    SearchEngine engine(bdd, &heuristic, profiler, targets, !BDDNoReorder, peek,
                        ShowBacktrack);
    return engine.search();
  } break;
  case HeuristicOption::DFS: {
    DFS heuristic;
    SearchEngine engine(bdd, &heuristic, profiler, targets, !BDDNoReorder, peek,
                        ShowBacktrack);
    return engine.search();
  } break;
  case HeuristicOption::RANDOM: {
    Random heuristic;
    SearchEngine engine(bdd, &heuristic, profiler, targets, !BDDNoReorder, peek,
                        ShowBacktrack);
    return engine.search();
  } break;
  case HeuristicOption::GALLIUM: {
    Gallium heuristic;
    SearchEngine engine(bdd, &heuristic, profiler, targets, !BDDNoReorder, peek,
                        ShowBacktrack);
    return engine.search();
  } break;
  case HeuristicOption::MAX_THROUGHPUT: {
    MaxThroughput heuristic;
    SearchEngine engine(bdd, &heuristic, profiler, targets, !BDDNoReorder, peek,
                        ShowBacktrack);
    return engine.search();
  } break;
  }

  assert(false && "Unknown heuristic");
}

std::string nf_name_from_bdd(const std::string &bdd_fname) {
  std::string nf_name = bdd_fname;
  nf_name = nf_name.substr(nf_name.find_last_of("/") + 1);
  nf_name = nf_name.substr(0, nf_name.find_last_of("."));
  return nf_name;
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  if (Verbose) {
    Log::MINIMUM_LOG_LEVEL = Log::Level::DEBUG;
  } else {
    Log::MINIMUM_LOG_LEVEL = Log::Level::LOG;
  }

  bdd::BDD *bdd = new bdd::BDD(InputBDDFile);

  unsigned seed = (Seed >= 0) ? Seed : std::random_device()();
  RandomEngine::seed(seed);

  Profiler *profiler = nullptr;
  if (!BDDProfile.empty()) {
    profiler = new Profiler(bdd, BDDProfile);
  } else {
    profiler = new Profiler(bdd);
  }

  profiler->log_debug();
  // ProfilerVisualizer::visualize(bdd, profiler, true);

  targets_t targets = build_targets(profiler);

  // std::string nf_name = nf_name_from_bdd(InputBDDFile);
  search_report_t report = search(bdd, profiler, targets);

  Log::log() << "\n";
  Log::log() << "Params:\n";
  Log::log() << "  Heuristic:        " << report.config.heuristic << "\n";
  Log::log() << "  Random seed:      " << seed << "\n";
  Log::log() << "Search:\n";
  Log::log() << "  Search time:      " << report.meta.elapsed_time << " s\n";
  Log::log() << "  SS size:          " << int2hr(report.meta.ss_size) << "\n";
  Log::log() << "  Steps:            " << int2hr(report.meta.steps) << "\n";
  Log::log() << "  Backtracks:       " << int2hr(report.meta.backtracks)
             << "\n";
  Log::log() << "  Branching factor: " << report.meta.branching_factor << "\n";
  Log::log() << "  Avg BDD size:     " << int2hr(report.meta.avg_bdd_size)
             << "\n";
  Log::log() << "  Solutions:        " << int2hr(report.meta.solutions) << "\n";
  Log::log() << "Winner EP:\n";
  Log::log() << "  Winner:           " << report.solution.score << "\n";
  Log::log() << "  Throughput:       " << report.solution.throughput_estimation
             << "\n";
  Log::log() << "  Speculation:      " << report.solution.throughput_speculation
             << "\n";
  Log::log() << "\n";

  if (ShowEP) {
    EPVisualizer::visualize(report.solution.ep, false);
  }

  if (ShowSS) {
    SSVisualizer::visualize(report.solution.search_space, report.solution.ep,
                            false);
  }

  if (ShowBDD) {
    bdd::BDDVisualizer::visualize(report.solution.ep->get_bdd(), false);
  }

  // synthesize(report.solution.ep, std::string(Out));

  if (report.solution.ep) {
    delete report.solution.ep;
  }

  if (report.solution.search_space) {
    delete report.solution.search_space;
  }

  delete bdd;

  delete_targets(targets);

  return 0;
}
