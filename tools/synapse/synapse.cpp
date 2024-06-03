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

llvm::cl::opt<bool> BDDReorder("r", llvm::cl::desc("Activate BDD reordering."),
                               llvm::cl::ValueDisallowed, llvm::cl::init(false),
                               llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowEP("s", llvm::cl::desc("Show winner Execution Plan."),
                           llvm::cl::ValueDisallowed, llvm::cl::init(false),
                           llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> ShowSS("ss",
                           llvm::cl::desc("Show the entire search space."),
                           llvm::cl::ValueDisallowed, llvm::cl::init(false),
                           llvm::cl::cat(SyNAPSE));

llvm::cl::list<int>
    Peek("ep-peek",
         llvm::cl::desc("Peek search space at these Execution Plans."),
         llvm::cl::Positional, llvm::cl::ZeroOrMore, llvm::cl::cat(SyNAPSE));

llvm::cl::opt<bool> Verbose("v", llvm::cl::desc("Verbose mode."),
                            llvm::cl::ValueDisallowed, llvm::cl::init(false),
                            llvm::cl::cat(SyNAPSE));
} // namespace

search_product_t search(const bdd::BDD &bdd) {
  Biggest biggest;
  DFS dfs;
  MostCompact most_compact;
  LeastReordered least_reordered;
  MaximizeSwitchNodes maximize_switch_nodes;
  Gallium gallium;

  std::unordered_set<ep_id_t> eps_to_peek;
  for (ep_id_t ep_id : Peek) {
    eps_to_peek.insert(ep_id);
  }

  SearchEngine engine(bdd, gallium, BDDReorder, eps_to_peek);
  search_product_t result = engine.search();

  return result;
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

  bdd::BDD bdd(InputBDDFile);
  std::string nf_name = nf_name_from_bdd(InputBDDFile);

  auto start_search = std::chrono::steady_clock::now();
  search_product_t result = search(bdd);
  auto end_search = std::chrono::steady_clock::now();

  auto search_dt = std::chrono::duration_cast<std::chrono::seconds>(
                       end_search - start_search)
                       .count();

  int64_t synthesis_dt = -1;

  // if (Out.size()) {
  //   auto start_synthesis = std::chrono::steady_clock::now();
  //   synthesize(nf_name, search_results.first);
  //   auto end_synthesis = std::chrono::steady_clock::now();

  //   synthesis_dt = std::chrono::duration_cast<std::chrono::seconds>(
  //                      end_synthesis - start_synthesis)
  //                      .count();
  // }

  Log::log() << "Search time:     " << search_dt << " sec\n";

  if (synthesis_dt >= 0) {
    Log::log() << "Generation time: " << synthesis_dt << " sec\n";
  }

  if (ShowEP) {
    EPVisualizer::visualize(result.ep, false);
  }

  if (ShowSS) {
    SSVisualizer::visualize(result.search_space, result.ep, false);
  }

  return 0;
}
