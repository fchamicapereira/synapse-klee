#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"

#include <fstream>

#include "call-paths-to-bdd.h"

namespace {
llvm::cl::list<std::string> InputCallPathFiles(llvm::cl::desc("<call paths>"),
                                               llvm::cl::Positional);

llvm::cl::OptionCategory BDDGeneratorCat("BDD generator specific options");

llvm::cl::opt<std::string>
    InputBDDFile("in", llvm::cl::desc("Input file for BDD deserialization."),
                 llvm::cl::cat(BDDGeneratorCat));

llvm::cl::opt<std::string>
    OutputBDDFile("out", llvm::cl::desc("Output file for BDD serialization."),
                  llvm::cl::cat(BDDGeneratorCat));
} // namespace

using namespace bdd;

void assert_bdd(const BDD &bdd) {
  std::cerr << "Asserting BDD...\n";

  const Node *root = bdd.get_root();
  assert(root);

  std::vector<const Node *> nodes{root};

  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    if (node->get_type() == bdd::NodeType::BRANCH) {
      const Branch *branch = static_cast<const bdd::Branch *>(node);

      const Node *on_true = branch->get_on_true();
      const Node *on_false = branch->get_on_false();

      assert(on_true);
      assert(on_false);

      const Node *on_true_prev = on_true->get_prev();
      const Node *on_false_prev = on_false->get_prev();

      assert(on_true_prev);
      assert(on_false_prev);

      assert(on_true_prev->get_id() == node->get_id());
      assert(on_false_prev->get_id() == node->get_id());

      nodes.push_back(on_true);
      nodes.push_back(on_false);
    } else {
      const Node *next = node->get_next();

      if (!next)
        continue;

      const Node *next_prev = next->get_prev();

      assert(next_prev);
      assert(next_prev->get_id() == node->get_id());

      nodes.push_back(next);
    }
  }

  std::cerr << "OK!\n";
}

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  if (InputBDDFile.size() == 0 && InputCallPathFiles.size() == 0) {
    std::cerr << "No input files provided.\n";
    return 1;
  }

  BDD bdd = InputBDDFile.size() ? BDD(InputBDDFile)
                                : BDD(call_paths_t(InputCallPathFiles));
  assert_bdd(bdd);

  PrinterDebug printer;
  bdd.visit(printer);

  if (OutputBDDFile.size())
    bdd.serialize(OutputBDDFile);

  return 0;
}
