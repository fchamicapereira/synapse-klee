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

#include "bdd-reorderer.h"
#include "bdd-visualizer.h"
#include "klee-util.h"

namespace {
llvm::cl::OptionCategory BDDReorderer("BDDReorderer specific options");

llvm::cl::opt<std::string>
    InputBDDFile("in", llvm::cl::desc("Input file for BDD deserialization."),
                 llvm::cl::Required, llvm::cl::cat(BDDReorderer));

llvm::cl::opt<int> MaxReorderingOperations(
    "max", llvm::cl::desc("Maximum number of reordering operations."),
    llvm::cl::initializer<int>(-1), llvm::cl::cat(BDDReorderer));

llvm::cl::opt<bool>
    Approximate("approximate",
                llvm::cl::desc("Get a lower bound approximation."),
                llvm::cl::ValueDisallowed, llvm::cl::init(false),
                llvm::cl::cat(BDDReorderer));

llvm::cl::opt<bool> Show("show", llvm::cl::desc("Show reordered BDDs."),
                         llvm::cl::ValueDisallowed, llvm::cl::init(false),
                         llvm::cl::cat(BDDReorderer));

llvm::cl::opt<std::string> ReportFile("report",
                                      llvm::cl::desc("Output report file"),
                                      llvm::cl::cat(BDDReorderer));
} // namespace

using namespace bdd;

void test(const BDD &bdd) {
  node_id_t anchor_id = 4;
  node_id_t proposed_candidate_id = 58;

  const Node *anchor = bdd.get_node_by_id(anchor_id);
  const Node *proposed_candidate = bdd.get_node_by_id(proposed_candidate_id);

  assert(anchor != nullptr && "Anchor node not found");
  assert(proposed_candidate != nullptr && "Proposed candidate not found");

  std::cerr << "Anchor: " << anchor->dump() << "\n";
  std::cerr << "Proposed candidate: " << proposed_candidate->dump() << "\n";

  candidate_t candidate;
  bool success = concretize_reordering_candidate(
      bdd, anchor_id, proposed_candidate_id, candidate);

  if (!success) {
    std::cerr << "Failed to concretize reordering candidate "
              << proposed_candidate_id << "\n";
    return;
  }

  std::cerr << "Concretized candidate: " << candidate.candidate_id << "\n";
  if (candidate.siblings.size() > 0) {
    std::cerr << "Siblings: ";
    for (node_id_t sibling : candidate.siblings) {
      std::cerr << sibling << " ";
    }
  }
  std::cerr << "\n";
  if (!candidate.condition.isNull())
    std::cerr << "Condition: "
              << kutil::expr_to_string(candidate.condition, true) << "\n";
  if (!candidate.extra_condition.isNull())
    std::cerr << "Extra condition: "
              << kutil::expr_to_string(candidate.extra_condition, true) << "\n";

  // std::vector<BDD> reordered_bdds = reorder(bdd, anchor_id);
  // std::cerr << "Reordered BDDs: " << reordered_bdds.size() << "\n";

  // for (const BDD &reordered_bdd : reordered_bdds) {
  //   GraphvizGenerator::visualize(reordered_bdd);
  // }
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  auto start = std::chrono::steady_clock::now();
  BDD original_bdd(InputBDDFile);

  test(original_bdd);

  // if (Approximate) {
  //   auto approximation = approximate_total_reordered_bdds(original_bdd);
  //   auto end = std::chrono::steady_clock::now();
  //   auto elapsed = end - start;
  //   auto elapsed_seconds =
  //       std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

  //   std::cerr << "\n";
  //   std::cerr << "Approximately " << approximation << " BDDs generated\n";
  //   std::cerr << "Elapsed: " << elapsed_seconds << " seconds\n";

  //   return 0;
  // }

  // auto reordered_bdds =
  //     get_all_reordered_bdds(original_bdd, MaxReorderingOperations);

  // std::cerr << "\n";
  // std::cerr << "Final: " << reordered_bdds.size() << "\n";

  // if (Show) {
  //   for (const BDD &bdd : reordered_bdds)
  //     GraphvizGenerator::visualize(bdd, true);
  // }

  // if (ReportFile.size() == 0) {
  //   return 0;
  // }

  // auto end = std::chrono::steady_clock::now();
  // auto elapsed = end - start;
  // auto elapsed_seconds =
  //     std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
  // auto report = std::ofstream(ReportFile, std::ios::out);
  // assert(report.is_open() && "Unable to open report file");

  // report << "# time (s)\ttotal\n";
  // report << elapsed_seconds << "\t" << reordered_bdds.size() << "\n";
  // report.close();

  return 0;
}
