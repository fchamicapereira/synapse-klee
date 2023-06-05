#pragma once

#include "nodes/nodes.h"

#include <vector>

namespace BDD {

template <class T> inline const T *cast_node(Node_ptr node) = delete;

template <> inline const Call *cast_node<Call>(Node_ptr node) {
  if (node->get_type() != Node::NodeType::CALL) {
    return nullptr;
  }

  return static_cast<const Call *>(node.get());
}

template <> inline const Branch *cast_node<Branch>(Node_ptr node) {
  if (node->get_type() != Node::NodeType::BRANCH) {
    return nullptr;
  }

  return static_cast<const Branch *>(node.get());
}

template <> inline const ReturnInit *cast_node<ReturnInit>(Node_ptr node) {
  if (node->get_type() != Node::NodeType::RETURN_INIT) {
    return nullptr;
  }

  return static_cast<const ReturnInit *>(node.get());
}

template <>
inline const ReturnProcess *cast_node<ReturnProcess>(Node_ptr node) {
  if (node->get_type() != Node::NodeType::RETURN_PROCESS) {
    return nullptr;
  }

  return static_cast<const ReturnProcess *>(node.get());
}

std::vector<Node_ptr> get_call_nodes(Node_ptr root,
                                     const std::vector<std::string> &fnames);

unsigned extract_port(const Branch* branch);

} // namespace BDD