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

void print(const BDD *bdd, const reorder_op_t &op) {
  const anchor_info_t &anchor_info = op.anchor_info;
  const candidate_info_t &candidate_info = op.candidate_info;

  const Node *anchor = bdd->get_node_by_id(anchor_info.id);
  const Node *candidate = bdd->get_node_by_id(candidate_info.id);

  assert(anchor && "Anchor node not found");
  assert(candidate && "Proposed candidate not found");

  std::cerr << "\n==================================\n";

  std::cerr << "* Anchor:\n";
  std::cerr << "\t" << anchor->dump(true) << " -> " << anchor_info.direction
            << "\n";
  std::cerr << "* Candidate:\n";
  std::cerr << "\t" << candidate->dump(true) << "\n";

  if (candidate_info.siblings.size() > 0) {
    std::cerr << "* Siblings: ";
    for (node_id_t sibling : candidate_info.siblings) {
      std::cerr << sibling << " ";
    }
    std::cerr << "\n";
  }

  if (!candidate_info.condition.isNull()) {
    std::cerr << "* Condition: "
              << kutil::expr_to_string(candidate_info.condition, true) << "\n";
  }

  std::cerr << "==================================\n";
}

void list_candidates(const BDD *bdd, const anchor_info_t &anchor_info) {
  std::vector<reorder_op_t> ops = get_reorder_ops(bdd, anchor_info);

  std::cerr << "Available reordering operations: " << ops.size() << "\n";
  for (const reorder_op_t &op : ops) {
    print(bdd, op);
  }
}

void apply_reordering_ops(
    const BDD *bdd,
    const std::vector<std::pair<anchor_info_t, node_id_t>> &ops) {
  std::vector<BDD *> created_bdds;

  for (const std::pair<anchor_info_t, node_id_t> &op : ops) {
    anchor_info_t anchor_info = op.first;
    node_id_t candidate_id = op.second;

    std::cerr << "-> Reordering op:";
    std::cerr << " anchor=" << anchor_info.id;
    std::cerr << " candidate=" << candidate_id;
    std::cerr << "\n";

    reordered_bdd_t reordered_bdd = try_reorder(bdd, anchor_info, candidate_id);

    if (reordered_bdd.op.candidate_info.status !=
        ReorderingCandidateStatus::VALID) {
      std::cerr << "Reordering failed: "
                << status_to_string(reordered_bdd.op.candidate_info.status)
                << "\n";
      break;
    } else {
      assert(reordered_bdd.bdd);
      BDDVisualizer::visualize(reordered_bdd.bdd, true);

      created_bdds.push_back(reordered_bdd.bdd);
      bdd = reordered_bdd.bdd;
    }
  }

  for (BDD *created_bdd : created_bdds) {
    delete created_bdd;
  }
}

void apply_all_candidates(const BDD *bdd, node_id_t anchor_id) {
  auto start = std::chrono::steady_clock::now();

  std::vector<reordered_bdd_t> bdds = reorder(bdd, anchor_id);

  auto end = std::chrono::steady_clock::now();
  auto elapsed = end - start;
  auto elapsed_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

  std::cerr << "Total: " << bdds.size() << "\n";
  std::cerr << "Elapsed: " << elapsed_seconds << " seconds\n";

  for (const reordered_bdd_t &reordered_bdd : bdds) {
    std::cerr << "\n==================================\n";
    std::cerr << "Candidate: " << reordered_bdd.op.candidate_info.id << "\n";
    if (reordered_bdd.op2.has_value()) {
      std::cerr << "Candidate2: " << reordered_bdd.op2->candidate_info.id
                << "\n";
    }
    std::cerr << "==================================\n";

    // BDDVisualizer::visualize(reordered_bdd.bdd, true);
  }

  for (reordered_bdd_t &reordered_bdd : bdds) {
    delete reordered_bdd.bdd;
  }
}

void estimate(const BDD *bdd) {
  auto start = std::chrono::steady_clock::now();

  double approximation = estimate_reorder(bdd);

  auto end = std::chrono::steady_clock::now();
  auto elapsed = end - start;
  auto elapsed_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

  std::cerr << "Approximately " << approximation << " BDDs generated\n";
  std::cerr << "Elapsed: " << elapsed_seconds << " seconds\n";
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  BDD *bdd = new BDD(InputBDDFile);

  // list_candidates(bdd, {16, false});
  apply_reordering_ops(bdd, {
                                {{6, false}, 10},
                            });
  // test_reorder(bdd, {
  //                       {{1, true}, 10},
  //                       {{1, true}, 12},
  //                       {{1, true}, 122},
  //                   });
  // test_reorder(bdd, 3);
  // estimate(bdd);

  delete bdd;

  return 0;
}
