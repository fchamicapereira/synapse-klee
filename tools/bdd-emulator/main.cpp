#include "llvm/Support/CommandLine.h"

#include "bdd-emulator.h"

namespace {
llvm::cl::OptionCategory BDDEmulator("BDDEmulator specific options");

llvm::cl::opt<std::string> InputBDDFile("in", llvm::cl::desc("BDD."),
                                        llvm::cl::Required,
                                        llvm::cl::cat(BDDEmulator));

llvm::cl::opt<std::string> InputPcap("pcap", llvm::cl::desc("Pcap file."),
                                     llvm::cl::Required,
                                     llvm::cl::cat(BDDEmulator));

llvm::cl::opt<int> InputDevice("device",
                               llvm::cl::desc("Device that receives packets."),
                               llvm::cl::Required, llvm::cl::cat(BDDEmulator));

llvm::cl::opt<float> Rate("rate", llvm::cl::desc("Rate (Gbps)"),
                          llvm::cl::Optional, llvm::cl::init(0),
                          llvm::cl::cat(BDDEmulator));

llvm::cl::opt<int>
    Loops("loops", llvm::cl::desc("Number of loops (0 to never stop looping)"),
          llvm::cl::Optional, llvm::cl::init(1), llvm::cl::cat(BDDEmulator));

llvm::cl::opt<int> Expiration("expiration",
                              llvm::cl::desc("Expiration time (us)"),
                              llvm::cl::init(0), llvm::cl::Optional,
                              llvm::cl::cat(BDDEmulator));

llvm::cl::opt<std::string> OutputReport("out",
                                        llvm::cl::desc("Output report file."),
                                        llvm::cl::Optional,
                                        llvm::cl::cat(BDDEmulator));

llvm::cl::opt<std::string>
    OutputDot("dot", llvm::cl::desc("Output graphviz dot file."),
              llvm::cl::cat(BDDEmulator));

llvm::cl::opt<bool> Show("s", llvm::cl::desc("Render dot file."),
                         llvm::cl::ValueDisallowed, llvm::cl::init(false),
                         llvm::cl::cat(BDDEmulator));

llvm::cl::opt<bool>
    Warmup("warmup",
           llvm::cl::desc("Loop the pcap first to warmup state, and then do "
                          "another pass to retrieve metadata."),
           llvm::cl::ValueDisallowed, llvm::cl::init(false),
           llvm::cl::cat(BDDEmulator));
} // namespace

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  auto bdd = BDD::BDD(InputBDDFile);
  auto cfg = BDD::emulation::cfg_t();

  if (Rate > 0) {
    cfg.rate.first = true;
    cfg.rate.second = Rate;
  }

  if (Expiration > 0) {
    cfg.timeout.first = true;
    cfg.timeout.second = Expiration;
  }

  cfg.loops = Loops;
  cfg.warmup = Warmup;
  cfg.report = true;

  BDD::emulation::Emulator emulator(bdd, cfg);
  emulator.run(InputPcap, InputDevice);

  const auto &reporter = emulator.get_reporter();

  if (Show) {
    reporter.render_hit_rate_dot(false);
  }

  if (OutputReport.size() != 0) {
    auto hit_rate_report = std::ofstream(OutputReport, std::ios::out);

    if (!hit_rate_report.is_open()) {
      std::cerr << "Unable to open report file " << OutputReport << "\n";
      return 1;
    }

    reporter.dump_hit_rate_csv(hit_rate_report);
    hit_rate_report.close();
  }

  if (OutputDot.size()) {
    auto dot = std::ofstream(OutputDot, std::ios::out);

    if (!dot.is_open()) {
      std::cerr << "Unable to open dot file " << OutputDot << "\n";
      return 1;
    }

    reporter.dump_hit_rate_dot(dot);
    dot.close();
  }

  return 0;
}
