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
#include "heuristics/heuristics.h"
#include "search.h"

using namespace synapse;

namespace {
llvm::cl::OptionCategory SyNAPSE("SyNAPSE specific options");

llvm::cl::opt<std::string> InputBDDFile("in", llvm::cl::desc("Input BDD."),
                                        llvm::cl::cat(SyNAPSE));

llvm::cl::opt<std::string>
    Out("out", llvm::cl::desc("Output directory for every generated file."),
        llvm::cl::cat(SyNAPSE));

llvm::cl::opt<int> Seed("seed", llvm::cl::desc("Random seed."),
                        llvm::cl::ValueRequired, llvm::cl::Optional,
                        llvm::cl::init(-1), llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> BDDNoReorder("no-reorder",
                                 llvm::cl::desc("Deactivate BDD reordering."),
                                 llvm::cl::ValueDisallowed,
                                 llvm::cl::init(false), llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowEP("s", llvm::cl::desc("Show winner Execution Plan."),
                           llvm::cl::ValueDisallowed, llvm::cl::init(false),
                           llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowSS("ss",
                           llvm::cl::desc("Show the entire search space."),
                           llvm::cl::ValueDisallowed, llvm::cl::init(false),
                           llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowBDD("sbdd", llvm::cl::desc("Show the BDD's solution."),
                            llvm::cl::ValueDisallowed, llvm::cl::init(false),
                            llvm::cl::cat(SyNAPSE));

llvm::cl::list<int>
    Peek("peek", llvm::cl::desc("Peek search space at these Execution Plans."),
         llvm::cl::Positional, llvm::cl::ZeroOrMore, llvm::cl::cat(SyNAPSE));

llvm::cl::opt<std::string> BDDProfile("prof",
                                      llvm::cl::desc("BDD profile JSON."),
                                      llvm::cl::ValueRequired,
                                      llvm::cl::Optional,
                                      llvm::cl::cat(SyNAPSE));

enum HeuristicOption {
  HEURISTIC_BFS,
  HEURISTIC_GALLIUM,
  HEURISTIC_MAX_THROUGHPUT,
};

llvm::cl::opt<HeuristicOption> ChosenHeuristic(
    "heuristic", llvm::cl::desc("Chosen heuristic."),
    llvm::cl::values(clEnumValN(HEURISTIC_BFS, "bfs", "BFS"),
                     clEnumValN(HEURISTIC_GALLIUM, "gallium", "Gallium"),
                     clEnumValN(HEURISTIC_MAX_THROUGHPUT, "max-throughput",
                                "Maximize throughput"),
                     clEnumValEnd),
    llvm::cl::Required, llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> Verbose("v", llvm::cl::desc("Verbose mode."),
                            llvm::cl::ValueDisallowed, llvm::cl::init(false),
                            llvm::cl::cat(SyNAPSE));
} // namespace

search_report_t search(const bdd::BDD *bdd, Profiler *profiler) {
  unsigned seed = (Seed >= 0) ? Seed : std::random_device()();

  std::unordered_set<ep_id_t> peek;
  for (ep_id_t ep_id : Peek) {
    peek.insert(ep_id);
  }

  // A bit disgusting, but oh well...
  switch (ChosenHeuristic) {
  case HEURISTIC_BFS: {
    BFS heuristic(seed);
    SearchEngine engine(bdd, &heuristic, profiler, !BDDNoReorder, peek);
    return engine.search();
  } break;
  case HEURISTIC_GALLIUM: {
    Gallium heuristic(seed);
    SearchEngine engine(bdd, &heuristic, profiler, !BDDNoReorder, peek);
    return engine.search();
  } break;
  case HEURISTIC_MAX_THROUGHPUT: {
    MaxThroughput heuristic(seed);
    SearchEngine engine(bdd, &heuristic, profiler, !BDDNoReorder, peek);
    return engine.search();
  } break;
  }

  assert(false && "Unknown heuristic");
}

// void synthesize(const std::string &fname, const EP*ep) {
//   CodeGenerator code_generator(Out, fname);
//   code_generator.generate(ep);
// }

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

  Profiler *profiler = nullptr;
  if (!BDDProfile.empty()) {
    profiler = new Profiler(bdd, BDDProfile);
  } else {
    profiler = new Profiler(bdd, Seed);
  }

  profiler->log_debug();

  std::string nf_name = nf_name_from_bdd(InputBDDFile);

  search_report_t report = search(bdd, profiler);

  // int64_t synthesis_dt = -1;
  // if (Out.size()) {
  //   auto start_synthesis = std::chrono::steady_clock::now();
  //   synthesize(nf_name, search_results.first);
  //   auto end_synthesis = std::chrono::steady_clock::now();

  //   synthesis_dt = std::chrono::duration_cast<std::chrono::seconds>(
  //                      end_synthesis - start_synthesis)
  //                      .count();
  // }

  Log::log() << "\n";
  Log::log() << "Search report:\n";
  Log::log() << "  Heuristic:   " << report.heuristic_name << "\n";
  Log::log() << "  Random seed: " << report.random_seed << "\n";
  Log::log() << "  SS size:     " << report.ss_size << "\n";
  Log::log() << "  Winner:      " << report.winner_score << "\n";
  Log::log() << "  Search time: " << report.search_time << " s\n";
  Log::log() << "\n";

  // if (synthesis_dt >= 0) {
  //   Log::log() << "Generation time: " << synthesis_dt << " s\n";
  // }

  if (ShowEP) {
    EPVisualizer::visualize(report.ep, false);
  }

  if (ShowSS) {
    SSVisualizer::visualize(report.search_space, report.ep, false);
  }

  if (ShowBDD) {
    bdd::BDDVisualizer::visualize(report.ep->get_bdd(), false);
  }

  delete bdd;

  return 0;
}
