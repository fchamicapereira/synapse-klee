#include "klee-util.h"
#include "call-paths-to-bdd.h"

#include <vector>
#include <string>
#include <sstream>
#include <optional>

namespace synapse {
namespace tofino {

enum class ParserStateType { EXTRACTOR, CONDITION, TERMINATING };

struct ParserState {
  bdd::node_id_t id;
  ParserStateType type;

  ParserState(bdd::node_id_t _id, ParserStateType _type)
      : id(_id), type(_type) {}

  virtual ~ParserState() {}

  virtual std::string dump(int lvl = 0) const = 0;
  virtual ParserState *clone() const = 0;
  virtual void
  retrieve(std::unordered_map<bdd::node_id_t, ParserState *> &states) = 0;
};

struct ParserStateTerminate : public ParserState {
  bool accept;

  ParserStateTerminate(bdd::node_id_t _id, bool _accept)
      : ParserState(_id, ParserStateType::TERMINATING), accept(_accept) {}

  ParserStateTerminate(const ParserStateTerminate &other)
      : ParserStateTerminate(other.id, other.accept) {}

  std::string dump(int lvl = 0) const {
    std::stringstream ss;
    ss << std::string(lvl * 2, ' ');
    ss << "[" << id << "] ";
    ss << (accept ? "ACCEPT" : "REJECT");
    ss << "\n";
    return ss.str();
  }

  ParserState *clone() const { return new ParserStateTerminate(*this); }

  void retrieve(std::unordered_map<bdd::node_id_t, ParserState *> &states) {
    states[id] = this;
  }
};

struct ParserStateCondition : public ParserState {
  klee::ref<klee::Expr> condition;
  ParserState *on_true;
  ParserState *on_false;

  ParserStateCondition(bdd::node_id_t _id, klee::ref<klee::Expr> _condition)
      : ParserState(_id, ParserStateType::CONDITION), condition(_condition),
        on_true(nullptr), on_false(nullptr) {}

  ParserStateCondition(const ParserStateCondition &other)
      : ParserStateCondition(other.id, other.condition) {}

  std::string dump(int lvl = 0) const {
    std::stringstream ss;

    ss << std::string(lvl * 2, ' ');
    ss << "[" << id << "] ";
    ss << "if(" << kutil::expr_to_string(condition, true) << ")\n";

    lvl++;

    if (on_true) {
      ss << std::string(lvl * 2, ' ');
      ss << "true:\n";
      ss << on_true->dump(lvl + 1);
    }

    if (on_false) {
      ss << std::string(lvl * 2, ' ');
      ss << "false:\n";
      ss << on_false->dump(lvl + 1);
    }

    return ss.str();
  }

  ParserState *clone() const {
    ParserState *on_true_clone = nullptr;
    ParserState *on_false_clone = nullptr;

    if (on_true) {
      on_true_clone = on_true->clone();
    }

    if (on_false) {
      on_false_clone = on_false->clone();
    }

    ParserStateCondition *clone = new ParserStateCondition(*this);
    clone->on_true = on_true_clone;
    clone->on_false = on_false_clone;

    return clone;
  }

  void retrieve(std::unordered_map<bdd::node_id_t, ParserState *> &states) {
    states[id] = this;
    if (on_true)
      on_true->retrieve(states);
    if (on_false)
      on_false->retrieve(states);
  }
};

struct ParserStateExtractor : public ParserState {
  klee::ref<klee::Expr> hdr;
  ParserState *next;

  ParserStateExtractor(bdd::node_id_t _id, klee::ref<klee::Expr> _hdr)
      : ParserState(_id, ParserStateType::EXTRACTOR), hdr(_hdr), next(nullptr) {
  }

  ParserStateExtractor(const ParserStateExtractor &other)
      : ParserStateExtractor(other.id, other.hdr) {}

  std::string dump(int lvl = 0) const {
    std::stringstream ss;

    ss << std::string(lvl * 2, ' ');
    ss << "[" << id << "] ";
    ss << "extract(" << kutil::expr_to_string(hdr, true) << ")\n";

    lvl++;

    if (next) {
      ss << next->dump(lvl + 1);
    }

    return ss.str();
  }

  ParserState *clone() const {
    ParserState *next_clone = nullptr;

    if (next) {
      next_clone = next->clone();
    }

    ParserStateExtractor *clone = new ParserStateExtractor(*this);
    clone->next = next_clone;

    return clone;
  }

  void retrieve(std::unordered_map<bdd::node_id_t, ParserState *> &states) {
    states[id] = this;
    if (next)
      next->retrieve(states);
  }
};

class Parser {
private:
  ParserState *initial_state;
  std::unordered_map<bdd::node_id_t, ParserState *> states;

public:
  Parser() : initial_state(nullptr) {}

  Parser(const Parser &other) : initial_state(nullptr) {
    if (other.initial_state) {
      initial_state = other.initial_state->clone();
      initial_state->retrieve(states);
    }
  }

  ~Parser() {
    for (auto &kv : states) {
      delete kv.second;
    }
  }

  void add_extractor(bdd::node_id_t leaf_id, bdd::node_id_t id,
                     klee::ref<klee::Expr> hdr, std::optional<bool> direction) {
    ParserState *new_state = new ParserStateExtractor(id, hdr);
    add_state(leaf_id, new_state, direction);
  }

  void add_extractor(bdd::node_id_t id, klee::ref<klee::Expr> hdr) {
    ParserState *new_state = new ParserStateExtractor(id, hdr);
    add_state(new_state);
  }

  void add_condition(bdd::node_id_t leaf_id, bdd::node_id_t id,
                     klee::ref<klee::Expr> condition,
                     std::optional<bool> direction) {
    ParserState *new_state = new ParserStateCondition(id, condition);
    add_state(leaf_id, new_state, direction);
  }

  void add_condition(bdd::node_id_t id, klee::ref<klee::Expr> condition) {
    ParserState *new_state = new ParserStateCondition(id, condition);
    add_state(new_state);
  }

  void accept(bdd::node_id_t leaf_id, bdd::node_id_t id,
              std::optional<bool> direction) {
    ParserState *new_state = new ParserStateTerminate(id, true);
    add_state(leaf_id, new_state, direction);
  }

  void reject(bdd::node_id_t leaf_id, bdd::node_id_t id,
              std::optional<bool> direction) {
    ParserState *new_state = new ParserStateTerminate(id, false);
    add_state(leaf_id, new_state, direction);
  }

  std::string dump() const {
    std::stringstream ss;
    ss << "******  Parser ******\n";
    if (initial_state)
      ss << initial_state->dump();
    ss << "************************\n";
    return ss.str();
  }

private:
  void add_state(ParserState *new_state) {
    assert(!initial_state);
    assert(states.empty());

    initial_state = new_state;
    states[new_state->id] = new_state;

    std::cerr << "-> NEW PARSER STATE <-\n";
    std::cerr << dump() << std::endl;
  }

  void add_state(bdd::node_id_t leaf_id, ParserState *new_state,
                 std::optional<bool> direction) {
    assert(initial_state);
    assert(states.find(leaf_id) != states.end());
    assert(states.find(new_state->id) == states.end());

    states[new_state->id] = new_state;

    ParserState *leaf = states[leaf_id];

    switch (leaf->type) {
    case ParserStateType::EXTRACTOR: {
      assert(!direction.has_value());
      ParserStateExtractor *extractor =
          static_cast<ParserStateExtractor *>(leaf);
      assert(!extractor->next);
      extractor->next = new_state;
    } break;
    case ParserStateType::CONDITION: {
      assert(direction.has_value());
      ParserStateCondition *condition =
          static_cast<ParserStateCondition *>(leaf);
      if (*direction) {
        assert(!condition->on_true);
        condition->on_true = new_state;
      } else {
        assert(!condition->on_false);
        condition->on_false = new_state;
      }
    } break;
    case ParserStateType::TERMINATING: {
      assert(false && "Cannot add extractor to terminating state");
    } break;
    }

    std::cerr << "-> NEW PARSER STATE <-\n";
    std::cerr << dump() << std::endl;
  }
};

} // namespace tofino
} // namespace synapse