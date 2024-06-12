#include "hit_rate_tree.h"
#include "klee-util.h"
#include "bdd-analyzer-report.h"
#include "random_engine.h"

#include <iomanip>

namespace synapse {

HitRateNode::HitRateNode(klee::ref<klee::Expr> _constraint, float _fraction)
    : constraint(_constraint), fraction(_fraction), on_true(nullptr),
      on_false(nullptr), prev(nullptr) {}

HitRateNode::HitRateNode(klee::ref<klee::Expr> _constraint, float _fraction,
                         bdd::node_id_t _bdd_node_id)
    : constraint(_constraint), fraction(_fraction), bdd_node_id(_bdd_node_id),
      on_true(nullptr), on_false(nullptr), prev(nullptr) {}

HitRateNode::~HitRateNode() {
  if (on_true) {
    delete on_true;
    on_true = nullptr;
  }

  if (on_false) {
    delete on_false;
    on_false = nullptr;
  }
}

HitRateNode *HitRateNode::clone() const {
  HitRateNode *new_node = new HitRateNode(constraint, fraction);

  if (bdd_node_id) {
    new_node->bdd_node_id = bdd_node_id;
  }

  if (on_true) {
    new_node->on_true = on_true->clone();
    new_node->on_true->prev = new_node;
  }

  if (on_false) {
    new_node->on_false = on_false->clone();
    new_node->on_false->prev = new_node;
  }

  return new_node;
}

void HitRateNode::dump(int lvl) const {
  std::cerr << "<";
  std::cerr << "fraction=" << fraction;
  if (bdd_node_id) {
    std::cerr << ", ";
    std::cerr << "bdd_node=" << *bdd_node_id;
  } else {
    if (!constraint.isNull()) {
      std::cerr << ", ";
      std::cerr << "cond=" << kutil::expr_to_string(constraint, true);
    }
  }
  std::cerr << ">";
  std::cout << "\n";

  if (on_true) {
    lvl++;
    std::cerr << std::string(2 * lvl, ' ');
    std::cout << "[T] ";
    on_true->dump(lvl);
    lvl--;
  }

  if (on_false) {
    lvl++;
    std::cerr << std::string(2 * lvl, ' ');
    std::cout << "[F] ";
    on_false->dump(lvl);
    lvl--;
  }
}

static HitRateNode *build_hit_fraction_tree(const bdd::Node *node,
                                            const bdd_node_counters &counters,
                                            uint64_t max_count) {
  HitRateNode *result = nullptr;

  if (!node) {
    return nullptr;
  }

  node->visit_nodes([&counters, max_count, &result](const bdd::Node *node) {
    if (node->get_type() != bdd::NodeType::BRANCH) {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    const bdd::Branch *branch = static_cast<const bdd::Branch *>(node);

    klee::ref<klee::Expr> condition = branch->get_condition();
    uint64_t counter = counters.at(node->get_id());
    float fraction = static_cast<float>(counter) / max_count;
    bdd::node_id_t bdd_node_id = node->get_id();

    HitRateNode *new_node = new HitRateNode(condition, fraction, bdd_node_id);

    const bdd::Node *on_true = branch->get_on_true();
    const bdd::Node *on_false = branch->get_on_false();

    new_node->on_true = build_hit_fraction_tree(on_true, counters, max_count);
    new_node->on_false = build_hit_fraction_tree(on_false, counters, max_count);

    if (!new_node->on_true && on_true) {
      uint64_t on_true_counter = counters.at(on_true->get_id());
      float on_true_fraction = static_cast<float>(on_true_counter) / max_count;
      bdd::node_id_t bdd_node_id = on_true->get_id();
      new_node->on_true =
          new HitRateNode(nullptr, on_true_fraction, bdd_node_id);
    }

    if (!new_node->on_false && on_false) {
      uint64_t on_false_counter = counters.at(on_false->get_id());
      float on_false_fraction =
          static_cast<float>(on_false_counter) / max_count;
      bdd::node_id_t bdd_node_id = on_false->get_id();
      new_node->on_false =
          new HitRateNode(nullptr, on_false_fraction, bdd_node_id);
    }

    if (new_node->on_true) {
      new_node->on_true->prev = new_node;
    }

    if (new_node->on_false) {
      new_node->on_false->prev = new_node;
    }

    result = new_node;
    return bdd::NodeVisitAction::STOP;
  });

  return result;
}

static HitRateNode *
build_random_hit_fraction_tree(const bdd::Node *node,
                               RandomEngine &random_percent_engine,
                               float fraction = 1.0) {
  HitRateNode *result = nullptr;

  if (!node) {
    return nullptr;
  }

  node->visit_nodes([&random_percent_engine, fraction,
                     &result](const bdd::Node *node) {
    if (node->get_type() != bdd::NodeType::BRANCH) {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    const bdd::Branch *branch = static_cast<const bdd::Branch *>(node);

    klee::ref<klee::Expr> condition = branch->get_condition();
    bdd::node_id_t bdd_node_id = node->get_id();

    HitRateNode *new_node = new HitRateNode(condition, fraction, bdd_node_id);

    const bdd::Node *on_true = branch->get_on_true();
    const bdd::Node *on_false = branch->get_on_false();

    int relative_percent_on_true = random_percent_engine.generate();
    float relative_fraction_on_true =
        static_cast<float>(relative_percent_on_true) / 100.0;

    float on_true_fraction = fraction * relative_fraction_on_true;
    float on_false_fraction = fraction - on_true_fraction;

    new_node->on_true = build_random_hit_fraction_tree(
        on_true, random_percent_engine, on_true_fraction);
    new_node->on_false = build_random_hit_fraction_tree(
        on_false, random_percent_engine, on_false_fraction);

    if (!new_node->on_true && on_true) {
      new_node->on_true =
          new HitRateNode(nullptr, on_true_fraction, bdd_node_id);
    }

    if (!new_node->on_false && on_false) {
      new_node->on_false =
          new HitRateNode(nullptr, on_false_fraction, bdd_node_id);
    }

    if (new_node->on_true) {
      new_node->on_true->prev = new_node;
    }

    if (new_node->on_false) {
      new_node->on_false->prev = new_node;
    }

    result = new_node;
    return bdd::NodeVisitAction::STOP;
  });

  return result;
}

HitRateTree::HitRateTree(const bdd::BDD *bdd, unsigned random_seed)
    : root(nullptr) {
  const bdd::Node *bdd_root = bdd->get_root();
  RandomEngine random_percent_engine(random_seed, 1, 100);
  root = build_random_hit_fraction_tree(bdd_root, random_percent_engine);
}

HitRateTree::HitRateTree(const bdd::BDD *bdd,
                         const std::string &bdd_profile_fname)
    : root(nullptr) {
  bdd_profile_t bdd_profile = parse_bdd_profile(bdd_profile_fname);

  const bdd::Node *bdd_root = bdd->get_root();
  uint64_t max_count = bdd_profile.counters[bdd_root->get_id()];

  root = build_hit_fraction_tree(bdd_root, bdd_profile.counters, max_count);
}

HitRateTree::HitRateTree(const HitRateTree &other)
    : root(other.root ? other.root->clone() : nullptr) {}

HitRateTree::HitRateTree(HitRateTree &&other) : root(std::move(other.root)) {
  other.root = nullptr;
}

HitRateTree::~HitRateTree() {
  if (root) {
    delete root;
  }
}

HitRateNode *HitRateTree::get_node(const constraints_t &constraints) const {
  HitRateNode *current = root;

  for (klee::ref<klee::Expr> constraint : constraints) {
    if (!current) {
      return nullptr;
    }

    klee::ref<klee::Expr> not_constraint =
        kutil::solver_toolbox.exprBuilder->Not(constraint);

    assert(!current->constraint.isNull());

    bool on_true = kutil::solver_toolbox.are_exprs_always_equal(
        constraint, current->constraint);
    bool on_false = kutil::solver_toolbox.are_exprs_always_equal(
        not_constraint, current->constraint);

    if (on_true) {
      current = current->on_true;
    } else if (on_false) {
      current = current->on_false;
    } else {
      return nullptr;
    }
  }

  return current;
}

static void recursive_update_fractions(HitRateNode *node,
                                       float parent_old_fraction,
                                       float parent_new_fraction) {
  if (!node) {
    return;
  }

  assert(parent_old_fraction <= 1.0);
  assert(parent_new_fraction <= 1.0);
  assert(parent_new_fraction <= parent_old_fraction);

  float old_fraction = node->fraction;
  float new_fraction =
      parent_old_fraction > 0
          ? (parent_new_fraction / parent_old_fraction) * node->fraction
          : 0;

  node->fraction = new_fraction;

  recursive_update_fractions(node->on_true, old_fraction, new_fraction);
  recursive_update_fractions(node->on_false, old_fraction, new_fraction);
}

void HitRateTree::append(HitRateNode *node, klee::ref<klee::Expr> constraint,
                         float fraction) {
  assert(node);

  HitRateNode *parent = node->prev;
  assert(parent);

  HitRateNode *new_node = new HitRateNode(constraint, node->fraction);

  assert((parent->on_true == node) || (parent->on_false == node));
  if (parent->on_true == node) {
    parent->on_true = new_node;
  } else {
    parent->on_false = new_node;
  }

  new_node->prev = parent;

  new_node->on_true = node;
  new_node->on_false = node->clone();

  float fraction_on_true = fraction;
  float fraction_on_false = new_node->fraction - fraction;

  assert(fraction_on_true <= 1.0);
  assert(fraction_on_false <= 1.0);

  assert(fraction_on_true <= new_node->fraction);
  assert(fraction_on_false <= new_node->fraction);

  recursive_update_fractions(new_node->on_true, new_node->fraction,
                             fraction_on_true);
  recursive_update_fractions(new_node->on_false, new_node->fraction,
                             fraction_on_false);
}

void HitRateTree::insert(const constraints_t &constraints,
                         klee::ref<klee::Expr> constraint, float fraction) {
  HitRateNode *node = get_node(constraints);
  append(node, constraint, fraction);
}

void HitRateTree::insert_relative(const constraints_t &constraints,
                                  klee::ref<klee::Expr> constraint,
                                  float rel_fraction_on_true) {
  HitRateNode *node = get_node(constraints);
  assert(node);

  float fraction = rel_fraction_on_true * node->fraction;
  append(node, constraint, fraction);
}

std::optional<float>
HitRateTree::get_fraction(const constraints_t &constraints) const {
  HitRateNode *node = get_node(constraints);

  if (!node) {
    return std::nullopt;
  }

  return node->fraction;
}

void HitRateTree::dump() const {
  if (root) {
    root->dump();
  }
}

} // namespace synapse