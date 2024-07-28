#pragma once

#include "tofino/synthesizer.h"
#include <filesystem>

namespace synapse {

void synthesize(const EP *ep, const std::filesystem::path &out_dir) {
  const targets_t &targets = ep->get_targets();

  for (const Target *target : targets) {
    switch (target->type) {
    case TargetType::Tofino: {
      tofino::TofinoSynthesizer synthesizer(out_dir, ep->get_bdd());
      synthesizer.visit(ep);
    } break;
    case TargetType::TofinoCPU: {
      //   assert(false && "TODO");
      // TofinoCPUSynthesizer synthesizer(out_dir);
      // synthesizer.visit(ep);
    } break;
    case TargetType::x86: {
      //   assert(false && "TODO");
      // x86Synthesizer synthesizer(out_dir);
      // synthesizer.visit(ep);
    } break;
    }
  }
}

} // namespace synapse
