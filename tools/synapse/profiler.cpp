#include "profiler.h"
#include "klee-util.h"
#include "bdd-analyzer-report.h"
#include "random_engine.h"
#include "log.h"

#include <iomanip>

namespace synapse {

ProfilerNode::ProfilerNode(klee::ref<klee::Expr> _constraint, float _fraction)
    : constraint(_constraint), fraction(_fraction), on_true(nullptr),
      on_false(nullptr), prev(nullptr) {}

ProfilerNode::ProfilerNode(klee::ref<klee::Expr> _constraint, float _fraction,
                           bdd::node_id_t _bdd_node_id)
    : constraint(_constraint), fraction(_fraction), bdd_node_id(_bdd_node_id),
      on_true(nullptr), on_false(nullptr), prev(nullptr) {}

ProfilerNode::~ProfilerNode() {
  if (on_true) {
    delete on_true;
    on_true = nullptr;
  }

  if (on_false) {
    delete on_false;
    on_false = nullptr;
  }
}

ProfilerNode *ProfilerNode::clone(bool keep_bdd_info) const {
  ProfilerNode *new_node = new ProfilerNode(constraint, fraction);

  if (keep_bdd_info) {
    new_node->bdd_node_id = bdd_node_id;
  }

  if (on_true) {
    new_node->on_true = on_true->clone(keep_bdd_info);
    new_node->on_true->prev = new_node;
  }

  if (on_false) {
    new_node->on_false = on_false->clone(keep_bdd_info);
    new_node->on_false->prev = new_node;
  }

  return new_node;
}

void ProfilerNode::log_debug(int lvl) const {
  Log::dbg() << "<";
  Log::dbg() << "fraction=" << fraction;
  if (bdd_node_id) {
    Log::dbg() << ", ";
    Log::dbg() << "bdd_node=" << *bdd_node_id;
  } else {
    if (!constraint.isNull()) {
      Log::dbg() << ", ";
      Log::dbg() << "cond=" << kutil::expr_to_string(constraint, true);
    }
  }
  Log::dbg() << ">";
  Log::dbg() << "\n";

  if (on_true) {
    lvl++;

    // Log::dbg() << std::string(2 * lvl, '|');
    for (int i = 0; i < 2 * lvl; i++)
      Log::dbg() << ((i % 2 != 0) ? "|" : " ");

    Log::dbg() << "[T] ";
    on_true->log_debug(lvl);
    lvl--;
  }

  if (on_false) {
    lvl++;

    for (int i = 0; i < 2 * lvl; i++)
      Log::dbg() << ((i % 2 != 0) ? "|" : " ");
    // Log::dbg() << std::string(2 * lvl, '|');

    Log::dbg() << "[F] ";
    on_false->log_debug(lvl);
    lvl--;
  }
}

static ProfilerNode *build_hit_fraction_tree(const bdd::Node *node,
                                             const bdd_node_counters &counters,
                                             uint64_t max_count) {
  ProfilerNode *result = nullptr;

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

    ProfilerNode *new_node = new ProfilerNode(condition, fraction, bdd_node_id);

    const bdd::Node *on_true = branch->get_on_true();
    const bdd::Node *on_false = branch->get_on_false();

    new_node->on_true = build_hit_fraction_tree(on_true, counters, max_count);
    new_node->on_false = build_hit_fraction_tree(on_false, counters, max_count);

    if (!new_node->on_true && on_true) {
      uint64_t on_true_counter = counters.at(on_true->get_id());
      float on_true_fraction = static_cast<float>(on_true_counter) / max_count;
      bdd::node_id_t bdd_node_id = on_true->get_id();
      new_node->on_true =
          new ProfilerNode(nullptr, on_true_fraction, bdd_node_id);
    }

    if (!new_node->on_false && on_false) {
      uint64_t on_false_counter = counters.at(on_false->get_id());
      float on_false_fraction =
          static_cast<float>(on_false_counter) / max_count;
      bdd::node_id_t bdd_node_id = on_false->get_id();
      new_node->on_false =
          new ProfilerNode(nullptr, on_false_fraction, bdd_node_id);
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

static ProfilerNode *
build_random_hit_fraction_tree(const bdd::Node *node,
                               RandomEngine &random_percent_engine,
                               float parent_fraction = 1.0) {
  ProfilerNode *result = nullptr;

  if (!node) {
    return nullptr;
  }

  node->visit_nodes([&random_percent_engine, parent_fraction,
                     &result](const bdd::Node *node) {
    if (node->get_type() != bdd::NodeType::BRANCH) {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    const bdd::Branch *branch = static_cast<const bdd::Branch *>(node);

    klee::ref<klee::Expr> condition = branch->get_condition();
    bdd::node_id_t bdd_node_id = node->get_id();

    ProfilerNode *new_node =
        new ProfilerNode(condition, parent_fraction, bdd_node_id);

    const bdd::Node *on_true = branch->get_on_true();
    const bdd::Node *on_false = branch->get_on_false();

    int relative_percent_on_true = random_percent_engine.generate();
    float relative_fraction_on_true =
        static_cast<float>(relative_percent_on_true) / 100.0;

    float on_true_fraction = parent_fraction * relative_fraction_on_true;
    float on_false_fraction = parent_fraction - on_true_fraction;

    new_node->on_true = build_random_hit_fraction_tree(
        on_true, random_percent_engine, on_true_fraction);
    new_node->on_false = build_random_hit_fraction_tree(
        on_false, random_percent_engine, on_false_fraction);

    if (!new_node->on_true && on_true) {
      new_node->on_true =
          new ProfilerNode(nullptr, on_true_fraction, on_true->get_id());
    }

    if (!new_node->on_false && on_false) {
      new_node->on_false =
          new ProfilerNode(nullptr, on_false_fraction, on_false->get_id());
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

static int get_random_avg_pkt_bytes(unsigned random_seed) {
  int min_frame = 64;
  int max_frame = 1500;
  int preamble = 8;
  int ipg = 12;

  int min_size = preamble + min_frame + ipg;
  int max_size = preamble + max_frame + ipg;

  RandomEngine random_percent_engine(random_seed, min_size, max_size);
  return random_percent_engine.generate();
}

Profiler::Profiler(const bdd::BDD *bdd, unsigned random_seed)
    : root(nullptr), avg_pkt_bytes(get_random_avg_pkt_bytes(random_seed)) {
  const bdd::Node *bdd_root = bdd->get_root();

  RandomEngine random_percent_engine(random_seed, 1, 100);
  root = build_random_hit_fraction_tree(bdd_root, random_percent_engine);
}

Profiler::Profiler(const bdd::BDD *bdd, const std::string &bdd_profile_fname)
    : root(nullptr) {
  bdd_profile_t bdd_profile = parse_bdd_profile(bdd_profile_fname);

  const bdd::Node *bdd_root = bdd->get_root();
  uint64_t max_count = bdd_profile.counters[bdd_root->get_id()];

  root = build_hit_fraction_tree(bdd_root, bdd_profile.counters, max_count);
  avg_pkt_bytes = bdd_profile.avg_pkt_bytes;

  int min_frame = 64;
  int max_frame = 1500;
  int preamble = 8;
  int ipg = 12;

  int min_size = preamble + min_frame + ipg;
  int max_size = preamble + max_frame + ipg;

  assert(avg_pkt_bytes >= min_size);
  assert(avg_pkt_bytes <= max_size);
}

Profiler::Profiler(const Profiler &other)
    : root(other.root ? other.root->clone(true) : nullptr),
      avg_pkt_bytes(other.avg_pkt_bytes) {}

Profiler::Profiler(Profiler &&other)
    : root(std::move(other.root)), avg_pkt_bytes(other.avg_pkt_bytes) {
  other.root = nullptr;
}

Profiler::~Profiler() {
  if (root) {
    delete root;
  }
}

int Profiler::get_avg_pkt_bytes() const { return avg_pkt_bytes; }

ProfilerNode *Profiler::get_node(const constraints_t &constraints) const {
  ProfilerNode *current = root;

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

static void recursive_update_fractions(ProfilerNode *node,
                                       float parent_old_fraction,
                                       float parent_new_fraction) {
  if (!node) {
    return;
  }

  assert(parent_old_fraction >= 0.0);
  assert(parent_old_fraction <= 1.0);

  assert(parent_new_fraction >= 0.0);
  assert(parent_new_fraction <= 1.0);

  float old_fraction = node->fraction;
  float new_fraction =
      parent_old_fraction != 0
          ? (parent_new_fraction / parent_old_fraction) * node->fraction
          : 0;

  node->fraction = new_fraction;

  recursive_update_fractions(node->on_true, old_fraction, new_fraction);
  recursive_update_fractions(node->on_false, old_fraction, new_fraction);
}

void Profiler::replace_root(klee::ref<klee::Expr> constraint, float fraction) {
  ProfilerNode *new_node = new ProfilerNode(constraint, 1.0);

  ProfilerNode *node = root;
  root = new_node;

  new_node->on_true = node;
  new_node->on_false = node->clone(false);

  new_node->on_true->prev = new_node;
  new_node->on_false->prev = new_node;

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

void Profiler::append(ProfilerNode *node, klee::ref<klee::Expr> constraint,
                      float fraction) {
  assert(node);

  ProfilerNode *parent = node->prev;

  if (!parent) {
    replace_root(constraint, fraction);
    return;
  }

  ProfilerNode *new_node = new ProfilerNode(constraint, node->fraction);

  assert((parent->on_true == node) || (parent->on_false == node));
  if (parent->on_true == node) {
    parent->on_true = new_node;
  } else {
    parent->on_false = new_node;
  }

  new_node->prev = parent;

  new_node->on_true = node;
  new_node->on_false = node->clone(false);

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

void Profiler::remove(ProfilerNode *node) {
  assert(node);

  ProfilerNode *parent = node->prev;
  assert(parent && "Cannot remove the root node");

  // By removing the current node, the parent is no longer needed (its purpose
  // was to differentiate between the on_true and on_false nodes, but now only
  // one side is left).

  ProfilerNode *grandparent = parent->prev;
  ProfilerNode *sibling;

  if (parent->on_true == node) {
    sibling = parent->on_false;
    parent->on_false = nullptr;
  } else {
    sibling = parent->on_true;
    parent->on_true = nullptr;
  }

  float parent_fraction = parent->fraction;

  if (!grandparent) {
    // The parent is the root node.
    assert(root == parent);
    root = sibling;
  } else {
    if (grandparent->on_true == parent) {
      grandparent->on_true = sibling;
    } else {
      grandparent->on_false = sibling;
    }

    sibling->prev = grandparent;
  }

  delete parent;

  float old_fraction = sibling->fraction;
  float new_fraction = parent_fraction;

  sibling->fraction = new_fraction;

  recursive_update_fractions(sibling->on_true, old_fraction, new_fraction);
  recursive_update_fractions(sibling->on_false, old_fraction, new_fraction);
}

void Profiler::remove(const constraints_t &constraints) {
  ProfilerNode *node = get_node(constraints);
  remove(node);
}

void Profiler::insert(const constraints_t &constraints,
                      klee::ref<klee::Expr> constraint, float fraction) {
  ProfilerNode *node = get_node(constraints);
  append(node, constraint, fraction);
}

void Profiler::insert_relative(const constraints_t &constraints,
                               klee::ref<klee::Expr> constraint,
                               float rel_fraction_on_true) {
  ProfilerNode *node = get_node(constraints);
  assert(node);

  float fraction = rel_fraction_on_true * node->fraction;
  append(node, constraint, fraction);
}

std::optional<float>
Profiler::get_fraction(const constraints_t &constraints) const {
  ProfilerNode *node = get_node(constraints);

  if (!node) {
    return std::nullopt;
  }

  return node->fraction;
}

void Profiler::log_debug() const {
  Log::dbg() << "\n============== Hit Rate Tree ==============\n";
  if (root) {
    root->log_debug();
  }
  Log::dbg() << "===========================================\n\n";
}

} // namespace synapse