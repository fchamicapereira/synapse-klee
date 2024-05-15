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

void test_getting_all_candidates(const BDD &bdd,
                                 const anchor_info_t &anchor_info) {
  const Node *anchor = bdd.get_node_by_id(anchor_info.id);
  std::vector<candidate_info_t> candidates =
      get_reordering_candidates(bdd, anchor_info);

  std::cerr << "Anchor:\n";
  std::cerr << "\t" << anchor->dump(true) << "\n";
  std::cerr << "\n";

  for (const candidate_info_t &candidate_info : candidates) {
    std::cerr << "Concretized candidate: " << candidate_info.id << "\n";
    if (candidate_info.siblings.size() > 0) {
      std::cerr << "Siblings: ";
      for (node_id_t sibling : candidate_info.siblings) {
        std::cerr << sibling << " ";
      }
      std::cerr << "\n";
    }

    if (!candidate_info.condition.isNull())
      std::cerr << "Condition: "
                << kutil::expr_to_string(candidate_info.condition, true)
                << "\n";
    std::cerr << "\n";
  }
}

void test_reorder(const BDD &original_bdd,
                  const std::vector<std::pair<anchor_info_t, node_id_t>>
                      &anchor_candidate_pairs) {
  BDD bdd = original_bdd;

  for (const std::pair<anchor_info_t, node_id_t> &anchor_candidate_pair :
       anchor_candidate_pairs) {
    const anchor_info_t &anchor_info = anchor_candidate_pair.first;
    node_id_t candidate_id = anchor_candidate_pair.second;

    const Node *anchor = bdd.get_node_by_id(anchor_info.id);
    const Node *proposed_candidate = bdd.get_node_by_id(candidate_id);

    assert(anchor && "Anchor node not found");
    assert(proposed_candidate && "Proposed candidate not found");

    std::cerr << "Anchor:\n";
    std::cerr << "\t" << anchor->dump(true) << "\n";
    std::cerr << "Candidate:\n";
    std::cerr << "\t" << proposed_candidate->dump(true) << "\n";
    std::cerr << "\n";

    candidate_info_t candidate_info;
    bool success = concretize_reordering_candidate(
        bdd, anchor_info, candidate_id, candidate_info);

    if (!success) {
      std::cerr << "Failed to concretize reordering candidate " << candidate_id
                << "\n";
      return;
    }

    std::cerr << "Concretized candidate: " << candidate_info.id << "\n";
    if (candidate_info.siblings.size() > 0) {
      std::cerr << "Siblings: ";
      for (node_id_t sibling : candidate_info.siblings) {
        std::cerr << sibling << " ";
      }
      std::cerr << "\n";
    }

    if (!candidate_info.condition.isNull()) {
      std::cerr << "Condition: "
                << kutil::expr_to_string(candidate_info.condition, true)
                << "\n";
    }
    std::cerr << "\n";

    bdd = reorder(bdd, anchor_info, candidate_info);
  }

  GraphvizGenerator::visualize(bdd, false);
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  auto start = std::chrono::steady_clock::now();
  BDD original_bdd(InputBDDFile);

  // test_getting_all_candidates(original_bdd, {25, true});
  test_reorder(original_bdd, {
                                 {{0, false}, 3},
                                 {{3, false}, 14},
                                 {{14, false}, 25},
                                 {{25, true}, 9},
                             });

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
