#pragma once

#include "nodes/nodes.h"

#include <vector>

namespace bdd {

std::vector<const Call *>
get_call_nodes(const Node *root, const std::vector<std::string> &fnames);

} // namespace bdd