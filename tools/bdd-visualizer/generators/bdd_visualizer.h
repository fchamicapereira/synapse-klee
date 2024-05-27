#pragma once

#include <assert.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <math.h>
#include <unistd.h>

#include "call-paths-to-bdd.h"

#include "graphviz.h"

namespace bdd {

struct processed_t {
  std::unordered_set<node_id_t> nodes;
  const Node *next;

  processed_t() : next(nullptr) {}
};

struct bdd_visualizer_opts_t {
  std::string fname;
  std::unordered_map<node_id_t, std::string> colors_per_node;
  std::pair<bool, std::string> default_color;
  std::unordered_map<node_id_t, std::string> annotations_per_node;
  processed_t processed;
};

class BDDVisualizer : public BDDVisitor, public Graphviz {
protected:
  bdd_visualizer_opts_t opts;

public:
  BDDVisualizer(const bdd_visualizer_opts_t &_opts);
  BDDVisualizer();

  std::string get_color(const Node *node) const;

  static void visualize(const BDD &bdd, bool interrupt,
                        bdd_visualizer_opts_t opts = {});

  void visit(const BDD &bdd) override;
  void visitRoot(const Node *root) override;

  BDDVisitorAction visitBranch(const Branch *node) override;
  BDDVisitorAction visitCall(const Call *node) override;
  BDDVisitorAction visitRoute(const Route *node) override;

private:
  std::string get_gv_name(const Node *node) const {
    return std::to_string(node->get_id());
  }
};

} // namespace bdd
