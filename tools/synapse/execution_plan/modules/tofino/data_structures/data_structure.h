#pragma once

#include <unordered_set>
#include <vector>

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "../../../../generic.h"

namespace synapse {
namespace targets {
namespace tofino {

class DataStructure;
typedef std::shared_ptr<DataStructure> DataStructureRef;

class DataStructure {
public:
  enum Type {
    TABLE,
    INTEGER_ALLOCATOR,
    COUNTER,
  };

protected:
  Type type;
  std::unordered_set<addr_t> objs;
  std::unordered_set<BDD::node_id_t> nodes;

public:
  DataStructure(Type _type, const std::unordered_set<addr_t> &_objs,
                const std::unordered_set<addr_t> &_nodes)
      : type(_type), objs(_objs), nodes(_nodes) {}

  Type get_type() const { return type; }
  const std::unordered_set<addr_t> &get_objs() const { return objs; }
  const std::unordered_set<BDD::node_id_t> &get_nodes() const { return nodes; }

  virtual void add_objs(const std::unordered_set<addr_t> &other_objs) {
    objs.insert(other_objs.begin(), other_objs.end());
  }

  virtual void
  add_nodes(const std::unordered_set<BDD::node_id_t> &other_nodes) {
    nodes.insert(other_nodes.begin(), other_nodes.end());
  }

  bool implements(const std::unordered_set<addr_t> &other_objs) const {
    for (auto obj : other_objs) {
      if (objs.find(obj) == objs.end()) {
        return false;
      }
    }

    return true;
  }

  virtual bool equals(const DataStructure *other) const {
    if (!other) {
      return false;
    }

    if (type != other->type) {
      return false;
    }

    if (objs != other->get_objs()) {
      return false;
    }

    if (nodes != other->get_nodes()) {
      return false;
    }

    return true;
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
