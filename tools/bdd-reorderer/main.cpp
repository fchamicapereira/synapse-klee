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
} // namespace

using namespace bdd;

std::string status_to_string(ReorderingCandidateStatus status) {
  std::string str;
  switch (status) {
  case ReorderingCandidateStatus::VALID:
    str = "VALID";
    break;
  case ReorderingCandidateStatus::UNREACHABLE_CANDIDATE:
    str = "UNREACHABLE_CANDIDATE";
    break;
  case ReorderingCandidateStatus::CANDIDATE_FOLLOWS_ANCHOR:
    str = "CANDIDATE_FOLLOWS_ANCHOR";
    break;
  case ReorderingCandidateStatus::IO_CHECK_FAILED:
    str = "IO_CHECK_FAILED";
    break;
  case ReorderingCandidateStatus::RW_CHECK_FAILED:
    str = "RW_CHECK_FAILED";
    break;
  case ReorderingCandidateStatus::NOT_ALLOWED:
    str = "NOT_ALLOWED";
    break;
  case ReorderingCandidateStatus::CONFLICTING_ROUTING:
    str = "CONFLICTING_ROUTING";
    break;
  case ReorderingCandidateStatus::IMPOSSIBLE_CONDITION:
    str = "IMPOSSIBLE_CONDITION";
    break;
  }
  return str;
}

void print(const BDD &bdd, const anchor_info_t &anchor_info,
           const candidate_info_t &candidate_info) {
  const Node *anchor = bdd.get_node_by_id(anchor_info.id);
  const Node *candidate = bdd.get_node_by_id(candidate_info.id);

  assert(anchor && "Anchor node not found");
  assert(candidate && "Proposed candidate not found");

  std::cerr << "\n==================================\n";

  std::cerr << "Anchor:\n";
  std::cerr << "\t" << anchor->dump(true) << "\n";
  std::cerr << "Candidate:\n";
  std::cerr << "\t" << candidate->dump(true) << "\n";

  if (candidate_info.siblings.size() > 0) {
    std::cerr << "Siblings: ";
    for (node_id_t sibling : candidate_info.siblings) {
      std::cerr << sibling << " ";
    }
    std::cerr << "\n";
  }

  if (!candidate_info.condition.isNull()) {
    std::cerr << "Condition: "
              << kutil::expr_to_string(candidate_info.condition, true) << "\n";
  }

  std::cerr << "==================================\n";
}

void test_candidate(const BDD &bdd, const anchor_info_t &anchor_info,
                    node_id_t candidate_id) {
  candidate_info_t candidate_info =
      concretize_reordering_candidate(bdd, anchor_info, candidate_id);

  if (candidate_info.status != ReorderingCandidateStatus::VALID) {
    std::cerr << "Failed to concretize reordering candidate " << candidate_id
              << ": " << status_to_string(candidate_info.status) << "\n";
    return;
  }

  print(bdd, anchor_info, candidate_info);
}

void test_getting_all_candidates(const BDD &bdd,
                                 const anchor_info_t &anchor_info) {
  std::vector<candidate_info_t> candidates =
      get_reordering_candidates(bdd, anchor_info);

  for (const candidate_info_t &candidate_info : candidates) {
    print(bdd, anchor_info, candidate_info);
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

    candidate_info_t candidate_info =
        concretize_reordering_candidate(bdd, anchor_info, candidate_id);

    if (candidate_info.status != ReorderingCandidateStatus::VALID) {
      std::cerr << "Failed to concretize reordering candidate " << candidate_id
                << ": " << status_to_string(candidate_info.status) << "\n";
      return;
    }

    print(bdd, anchor_info, candidate_info);
    bdd = reorder(bdd, anchor_info, candidate_info);
  }

  GraphvizGenerator::visualize(bdd, false);
}

void test_get_all_reordered_bdds(const BDD &original_bdd,
                                 int max_reordering_operations) {
  auto start = std::chrono::steady_clock::now();

  std::vector<reordered_bdd_t> bdds = reorder(original_bdd);

  auto end = std::chrono::steady_clock::now();
  auto elapsed = end - start;
  auto elapsed_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

  std::cerr << "Total: " << bdds.size() << "\n";
  std::cerr << "Elapsed: " << elapsed_seconds << " seconds\n";

  // if (Show) {
  //   for (const BDD &bdd : bdds)
  //     GraphvizGenerator::visualize(bdd, true);
  // }
}

void test_reorder(const BDD &bdd, const anchor_info_t &anchor_info) {
  auto start = std::chrono::steady_clock::now();

  std::vector<reordered_bdd_t> bdds = reorder(bdd, anchor_info);

  auto end = std::chrono::steady_clock::now();
  auto elapsed = end - start;
  auto elapsed_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

  std::cerr << "Total: " << bdds.size() << "\n";
  std::cerr << "Elapsed: " << elapsed_seconds << " seconds\n";

  for (const reordered_bdd_t &reordered_bdd : bdds) {
    std::cerr << "Candidate: " << reordered_bdd.candidate_info.id << "\n";
    GraphvizGenerator::visualize(reordered_bdd.bdd, true);
  }
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  BDD bdd(InputBDDFile);

  // test_candidate(bdd, {3, true}, 5);
  // test_getting_all_candidates(bdd, {0, true});
  // test_reorder(bdd, {
  //                       {{1, true}, 10},
  //                       {{1, true}, 12},
  //                       {{1, true}, 122},
  //                   });
  // test_reorder(bdd, {0, true});

  auto approximation = estimate_reorder(bdd);
  std::cerr << "Approximately " << approximation << " BDDs generated\n";

  return 0;
}
