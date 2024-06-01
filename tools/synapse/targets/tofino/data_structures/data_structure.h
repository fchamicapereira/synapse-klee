#pragma once

#include "klee-util.h"
#include "call-paths-to-bdd.h"

#include <unordered_set>

namespace synapse {
namespace tofino {

typedef int DataStructureID;

enum class DSType {
  SIMPLE_TABLE,
};

struct DataStructure {
  DSType type;
  DataStructureID id;

  DataStructure(DSType _type, DataStructureID _id) : type(_type), id(_id) {}

  virtual ~DataStructure() {}
  virtual DataStructure *clone() const = 0;
};

} // namespace tofino
} // namespace synapse