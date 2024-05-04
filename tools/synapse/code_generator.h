#pragma once

#include "execution_plan/execution_plan.h"
#include "synthesizers/synthesizers.h"

#include <sys/stat.h>
#include <vector>

using synapse::synthesizer::Synthesizer;
using synapse::synthesizer::tofino::TofinoGenerator;
using synapse::synthesizer::x86::x86Generator;
using synapse::synthesizer::x86_tofino::x86TofinoGenerator;

namespace synapse {

class CodeGenerator {
private:
  typedef ExecutionPlan (CodeGenerator::*ExecutionPlanTargetExtractor)(
      const ExecutionPlan &) const;
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

  ExecutionPlan x86_extractor(const ExecutionPlan &execution_plan) const;
  ExecutionPlan x86_tofino_extractor(const ExecutionPlan &execution_plan) const;
  ExecutionPlan tofino_extractor(const ExecutionPlan &execution_plan) const;

  std::string directory;
  std::string fname;

public:
  CodeGenerator(const std::string &_directory, const std::string &_fname)
      : directory(_directory), fname(_fname) {
    target_helpers_bank = {
        {TargetType::x86_Tofino,
         target_helper_t(&CodeGenerator::x86_tofino_extractor,
                         std::make_shared<x86TofinoGenerator>())},

        {TargetType::Tofino,
         target_helper_t(&CodeGenerator::tofino_extractor,
                         std::make_shared<TofinoGenerator>())},

        {TargetType::x86, target_helper_t(&CodeGenerator::x86_extractor,
                                          std::make_shared<x86Generator>())},
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
    case TargetType::x86_Tofino:
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

  void generate(const ExecutionPlan &execution_plan) {
    for (auto helper : target_helpers_loaded) {
      auto &extractor = helper.extractor;
      auto &generator = helper.generator;

      auto extracted_ep = (this->*extractor)(execution_plan);
      generator->generate(extracted_ep, execution_plan);
    }
  }
};

} // namespace synapse