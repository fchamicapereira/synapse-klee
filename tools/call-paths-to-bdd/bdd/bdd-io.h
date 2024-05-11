#pragma once

#include <vector>
#include <string>

namespace bdd {

constexpr char MAGIC_SIGNATURE[] = "===== BDD =====";
constexpr char KQUERY_DELIMITER[] = ";;-- kQuery --";
constexpr char SYMBOLS_DELIMITER[] = ";;-- Symbols --";
constexpr char INIT_DELIMITER[] = ";;-- Init --";
constexpr char NODES_DELIMITER[] = ";; -- Nodes --";
constexpr char EDGES_DELIMITER[] = ";; -- Edges --";
constexpr char ROOT_DELIMITER[] = ";; -- Root --";

} // namespace bdd