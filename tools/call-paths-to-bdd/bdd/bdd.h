#pragma once

#include "nodes/node.h"
#include "nodes/manager.h"

namespace bdd {

class BDDVisitor;

class BDD {
private:
  node_id_t id;

  symbol_t device;
  symbol_t packet_len;
  symbol_t time;

  std::vector<call_t> init;
  Node *root;

  NodeManager manager;

public:
  BDD() : id(0), root(nullptr) {}

  BDD(const call_paths_t &call_paths);
  BDD(const std::string &file_path);

  BDD(BDD &&other)
      : id(other.id), device(std::move(other.device)),
        packet_len(std::move(other.packet_len)), time(std::move(other.time)),
        init(std::move(other.init)), root(other.root),
        manager(std::move(other.manager)) {
    other.root = nullptr;
  }

  BDD(const BDD &other)
      : id(other.id), device(other.device), packet_len(other.packet_len),
        time(other.time), init(other.init) {
    root = other.root->clone(manager, true);
  }

  BDD &operator=(const BDD &other) {
    if (this == &other)
      return *this;
    id = other.id;
    device = other.device;
    packet_len = other.packet_len;
    time = other.time;
    init = other.init;
    root = other.root->clone(manager, true);
    return *this;
  }

  node_id_t get_id() const { return id; }
  node_id_t &get_mutable_id() { return id; }

  symbol_t get_device() const { return device; }
  symbol_t get_packet_len() const { return packet_len; }
  symbol_t get_time() const { return time; }
  const std::vector<call_t> &get_init() const { return init; }
  const Node *get_root() const { return root; }

  std::string hash() const { return root->hash(true); }
  size_t size() const { return root->count_children(true) + 1; }

  symbols_t get_generated_symbols(const Node *node) const;
  void visit(BDDVisitor &visitor) const;
  void serialize(const std::string &file_path) const;
  void deserialize(const std::string &file_path);
  void inspect() const;

  const Node *get_node_by_id(node_id_t _id) const;
  Node *get_mutable_node_by_id(node_id_t _id);

  NodeManager &get_mutable_manager() { return manager; }
};

} // namespace bdd
