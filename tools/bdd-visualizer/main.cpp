#include "llvm/Support/CommandLine.h"

#include "bdd-visualizer.h"

namespace {
llvm::cl::OptionCategory BDDVisualizer("BDDVisualizer specific options");

llvm::cl::opt<std::string> InputBDDFile("in", llvm::cl::desc("BDD."),
                                        llvm::cl::Required,
                                        llvm::cl::cat(BDDVisualizer));

llvm::cl::opt<std::string>
    BDDAnalyzerReport("report", llvm::cl::desc("BDD analyzer report file."),
                      llvm::cl::Optional, llvm::cl::cat(BDDVisualizer));

llvm::cl::opt<std::string>
    OutputDot("out", llvm::cl::desc("Output graphviz dot file."),
              llvm::cl::cat(BDDVisualizer));

llvm::cl::opt<bool> Show("show", llvm::cl::desc("Render dot file."),
                         llvm::cl::ValueDisallowed, llvm::cl::init(false),
                         llvm::cl::cat(BDDVisualizer));

} // namespace

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  bdd::BDD bdd = bdd::BDD(InputBDDFile);

  if (BDDAnalyzerReport.size()) {
    bdd_analyzer_report_t report =
        parse_bdd_analyzer_report_t(BDDAnalyzerReport);

    if (OutputDot.size()) {
      bdd::HitRateGraphvizGenerator generator(OutputDot, report.counters);
      generator.visit(bdd);
    }

    if (Show) {
      bdd::HitRateGraphvizGenerator::visualize(bdd, report.counters, false);
    }
  } else {
    if (OutputDot.size()) {
      bdd::bdd_visualizer_opts_t opts;
      opts.fname = OutputDot;

      bdd::BDDVisualizer generator(opts);
      generator.visit(bdd);
      generator.write();
    }

    if (Show) {
      bdd::BDDVisualizer::visualize(bdd, false);
    }
  }

  return 0;
}
