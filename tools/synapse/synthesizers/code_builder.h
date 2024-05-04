#pragma once

#include "../log.h"

#include <assert.h>
#include <sstream>
#include <unordered_map>

namespace synapse {
namespace synthesizer {

class CodeBuilder {
private:
  std::stringstream ss;
  int lvl;

public:
  CodeBuilder() : lvl(0) {}
  CodeBuilder(int _lvl) : lvl(_lvl) {}

  void indent() {
    assert(lvl >= 0);
    ss << std::string(lvl * 2, ' ');
  }

  void inc_indentation() {
    assert(lvl >= 0);
    lvl++;
  }

  void dec_indentation() {
    assert(lvl > 0);
    lvl--;
  }

  void append(const std::string code) { ss << code; }
  void append(int n) { ss << n; }

  void append_new_line() { ss << "\n"; }

  std::string dump() const { return ss.str(); }
  void dump(std::ostream &os) const { os << ss.str(); }
};

} // namespace synthesizer
} // namespace synapse