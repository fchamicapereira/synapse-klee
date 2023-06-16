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

BDD::emulation::meta_t run(const BDD::BDD &bdd,
                           const BDD::emulation::cfg_t &cfg,
                           const std::string &pcap_file, uint16_t device) {
  BDD::emulation::Emulator emulator(bdd, cfg);
  emulator.run(pcap_file, device);
  return emulator.get_meta();
}

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

  cfg.warmup = Warmup;
  cfg.report = true;

  auto meta = run(bdd, cfg, InputPcap, InputDevice);
  std::cerr << meta << "\n";

  if (Show) {
    BDD::HitRateGraphvizGenerator::visualize(bdd, meta.get_hit_rate(), false);
  }

  if (OutputReport.size() != 0) {
    auto report = std::ofstream(OutputReport, std::ios::out);

    if (report.is_open()) {
      report.close();
    } else {
      std::cerr << "Unable to open report file " << OutputReport << "\n";
    }
  }

  return 0;
}
