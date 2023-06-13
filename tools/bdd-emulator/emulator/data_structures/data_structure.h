#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

namespace BDD {
namespace emulation {

enum DataStructureType { MAP, VECTOR, DCHAIN };

class DataStructure {
protected:
  DataStructureType type;
  addr_t obj;

public:
  DataStructure(DataStructureType _type, addr_t _obj)
      : type(_type), obj(_obj) {}

  DataStructureType get_type() const { return type; }
  addr_t get_obj() const { return obj; }
};

typedef std::shared_ptr<DataStructure> DataStructureRef;

} // namespace emulation
} // namespace BDD