#pragma once

#include "nodes/node.h"
#include "nodes/manager.h"

namespace bdd {

class BDDVisitor;

class BDD {
private:
  node_id_t id;

  klee::ref<klee::Expr> device;
  klee::ref<klee::Expr> packet_len;
  klee::ref<klee::Expr> time;

  std::vector<call_t> init;
  Node *root;

  NodeManager manager;

public:
  BDD() : id(0) {}

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

  BDD &operator=(const BDD &) = default;

  node_id_t get_id() const { return id; }
  klee::ref<klee::Expr> get_device() const { return device; }
  klee::ref<klee::Expr> get_packet_len() const { return packet_len; }
  klee::ref<klee::Expr> get_time() const { return time; }
  const std::vector<call_t> &get_init() const { return init; }
  const Node *get_root() const { return root; }

  std::string hash() const { return root->hash(true); }

  void visit(BDDVisitor &visitor) const;
  void serialize(const std::string &file_path) const;
  void deserialize(const std::string &file_path);

  const Node *get_node_by_id(node_id_t _id) const;

  Node *get_mutable_node_by_id(node_id_t _id);
  NodeManager &get_mutable_manager() { return manager; }
};

} // namespace bdd
