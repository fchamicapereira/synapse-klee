#pragma once

#include <unordered_map>
#include <sstream>
#include <filesystem>

#include "../execution_plan/execution_plan.h"
#include "../execution_plan/visitor.h"

namespace synapse {

typedef int indent_t;
typedef std::string marker_t;
typedef std::string code_t;

struct code_builder_t {
  std::stringstream stream;
  indent_t lvl;

  code_builder_t(indent_t lvl = 0);

  void inc();
  void dec();
  void indent();
  code_t dump() const;

  code_builder_t &operator<<(const code_t &code);
  code_builder_t &operator<<(int n);
};

class Synthesizer : public EPVisitor {
private:
  const std::filesystem::path template_file;
  const std::filesystem::path out_file;
  std::unordered_map<marker_t, code_builder_t> builders;

public:
  Synthesizer(const char *template_fname,
              const std::unordered_map<marker_t, indent_t> &markers,
              const std::filesystem::path &out_file);

  virtual void log(const EPNode *ep_node) const override {
    // Don't log anything.
  }

protected:
  code_builder_t &get(const marker_t &marker);
  void dump() const;
  void dbg() const;
};

} // namespace synapse