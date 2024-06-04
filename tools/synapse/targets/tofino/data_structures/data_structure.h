#pragma once

#include "klee-util.h"
#include "call-paths-to-bdd.h"

#include <unordered_set>

namespace synapse {
namespace tofino {

typedef int DS_ID;

enum class DSType {
  SIMPLE_TABLE,
};

struct DS {
  DSType type;
  DS_ID id;

  DS(DSType _type, DS_ID _id) : type(_type), id(_id) {}

  virtual ~DS() {}
  virtual DS *clone() const = 0;
};

} // namespace tofino
} // namespace synapse