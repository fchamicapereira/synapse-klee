#pragma once

#include "execution_plan/execution_plan.h"
#include "synthesizers/synthesizers.h"

#include <sys/stat.h>
#include <vector>

using synapse::Synthesizer;
using synapse::tofino::TofinoSynthesizer;
using synapse::tofino_cpu::TofinoCPUSynthesizer;
using synapse::x86::x86Synthesizer;

namespace synapse {

class CodeGenerator {
private:
  typedef EP (CodeGenerator::*ExecutionPlanTargetExtractor)(const EP &) const;
  typedef std::shared_ptr<Synthesizer> Synthesizer_ptr;

  struct target_helper_t {
    ExecutionPlanTargetExtractor extractor;
    Synthesizer_ptr generator;

    target_helper_t(ExecutionPlanTargetExtractor _extractor)
        : extractor(_extractor) {}

    target_helper_t(ExecutionPlanTargetExtractor _extractor,
                    Synthesizer_ptr _generator)
        : extractor(_extractor), generator(_generator) {}
  };

  std::vector<target_helper_t> target_helpers_loaded;
  std::map<TargetType, target_helper_t> target_helpers_bank;

  EP x86_extractor(const EP &execution_plan) const;
  EP tofino_cpu_extractor(const EP &execution_plan) const;
  EP tofino_extractor(const EP &execution_plan) const;

  std::string directory;
  std::string fname;

public:
  CodeGenerator(const std::string &_directory, const std::string &_fname)
      : directory(_directory), fname(_fname) {
    target_helpers_bank = {
        {TargetType::TofinoCPU,
         target_helper_t(&CodeGenerator::tofino_cpu_extractor,
                         std::make_shared<TofinoCPUSynthesizer>())},

        {TargetType::Tofino,
         target_helper_t(&CodeGenerator::tofino_extractor,
                         std::make_shared<TofinoSynthesizer>())},

        {TargetType::x86, target_helper_t(&CodeGenerator::x86_extractor,
                                          std::make_shared<x86Synthesizer>())},
    };
  }

  void add_target(TargetType target) {
    auto found_it = target_helpers_bank.find(target);
    assert(found_it != target_helpers_bank.end() &&
           "TargetType not found in target_extractors_bank of CodeGenerator");
    assert(found_it->second.generator);

    if (!directory.size()) {
      target_helpers_loaded.push_back(found_it->second);
      return;
    }

    auto output_file = directory + "/" + fname + "-";

    switch (target) {
    case TargetType::TofinoCPU:
      output_file += "tofino-cpu.cpp";
      break;
    case TargetType::Tofino:
      output_file += "tofino-asic.p4";
      break;
    case TargetType::x86:
      output_file += "x86.cpp";
      break;
    }

    found_it->second.generator->output_to_file(output_file);
    target_helpers_loaded.push_back(found_it->second);
  }

  void generate(const EP &execution_plan) {
    for (auto helper : target_helpers_loaded) {
      auto &extractor = helper.extractor;
      auto &generator = helper.generator;

      auto extracted_ep = (this->*extractor)(execution_plan);
      generator->generate(extracted_ep, execution_plan);
    }
  }
};

} // namespace synapse