#include "klee-util.h"
#include "call-paths-to-bdd.h"

#include <vector>
#include <string>
#include <sstream>
#include <optional>

namespace synapse {
namespace tofino {

struct ParserState;
typedef std::unordered_map<bdd::node_id_t, ParserState *> states_t;

enum class ParserStateType { EXTRACT, SELECT, TERMINATE };

struct ParserState {
  bdd::node_id_t id;
  ParserStateType type;

  ParserState(bdd::node_id_t _id, ParserStateType _type)
      : id(_id), type(_type) {}

  virtual ~ParserState() {}

  virtual std::string dump(int lvl = 0) const = 0;
  virtual ParserState *clone() const = 0;
  virtual void retrieve(states_t &states) = 0;
};

struct ParserStateTerminate : public ParserState {
  bool accept;

  ParserStateTerminate(bdd::node_id_t _id, bool _accept)
      : ParserState(_id, ParserStateType::TERMINATE), accept(_accept) {}

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

  void retrieve(states_t &states) { states[id] = this; }
};

struct ParserStateSelect : public ParserState {
  klee::ref<klee::Expr> field;
  std::vector<int> values;
  ParserState *on_true;
  ParserState *on_false;

  ParserStateSelect(bdd::node_id_t _id, klee::ref<klee::Expr> _field,
                    const std::vector<int> &_values)
      : ParserState(_id, ParserStateType::SELECT), field(_field),
        values(_values), on_true(nullptr), on_false(nullptr) {}

  ParserStateSelect(const ParserStateSelect &other)
      : ParserStateSelect(other.id, other.field, other.values) {}

  std::string dump(int lvl = 0) const {
    std::stringstream ss;

    ss << std::string(lvl * 2, ' ');
    ss << "[" << id << "] ";
    ss << "select (";
    ss << "field=";
    ss << kutil::expr_to_string(field, true);
    ss << ", values=[";
    for (size_t i = 0; i < values.size(); i++) {
      ss << values[i];
      if (i < values.size() - 1)
        ss << ", ";
    }
    ss << "]";
    ss << ")\n";

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

    ParserStateSelect *clone = new ParserStateSelect(*this);
    clone->on_true = on_true_clone;
    clone->on_false = on_false_clone;

    return clone;
  }

  void retrieve(states_t &states) {
    states[id] = this;
    if (on_true)
      on_true->retrieve(states);
    if (on_false)
      on_false->retrieve(states);
  }
};

struct ParserStateExtract : public ParserState {
  klee::ref<klee::Expr> hdr;
  ParserState *next;

  ParserStateExtract(bdd::node_id_t _id, klee::ref<klee::Expr> _hdr)
      : ParserState(_id, ParserStateType::EXTRACT), hdr(_hdr), next(nullptr) {}

  ParserStateExtract(const ParserStateExtract &other)
      : ParserStateExtract(other.id, other.hdr) {}

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

    ParserStateExtract *clone = new ParserStateExtract(*this);
    clone->next = next_clone;

    return clone;
  }

  void retrieve(states_t &states) {
    states[id] = this;
    if (next)
      next->retrieve(states);
  }
};

class Parser {
private:
  ParserState *initial_state;
  states_t states;

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

  void add_extract(bdd::node_id_t leaf_id, bdd::node_id_t id,
                   klee::ref<klee::Expr> hdr, std::optional<bool> direction) {
    ParserState *new_state = new ParserStateExtract(id, hdr);
    add_state(leaf_id, new_state, direction);
  }

  void add_extract(bdd::node_id_t id, klee::ref<klee::Expr> hdr) {
    ParserState *new_state = new ParserStateExtract(id, hdr);
    add_state(new_state);
  }

  void add_select(bdd::node_id_t leaf_id, bdd::node_id_t id,
                  klee::ref<klee::Expr> field, const std::vector<int> &values,
                  std::optional<bool> direction) {
    ParserStateSelect *new_state = new ParserStateSelect(id, field, values);
    add_state(leaf_id, new_state, direction);
  }

  void add_select(bdd::node_id_t id, klee::ref<klee::Expr> field,
                  const std::vector<int> &values) {
    ParserState *new_state = new ParserStateSelect(id, field, values);
    add_state(new_state);
  }

  void accept(bdd::node_id_t leaf_id, bdd::node_id_t id,
              std::optional<bool> direction) {
    if (already_terminated(leaf_id, id, direction, true)) {
      return;
    }

    ParserState *new_state = new ParserStateTerminate(id, true);
    add_state(leaf_id, new_state, direction);
  }

  void reject(bdd::node_id_t leaf_id, bdd::node_id_t id,
              std::optional<bool> direction) {
    if (already_terminated(leaf_id, id, direction, false)) {
      return;
    }

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
  bool already_terminated(bdd::node_id_t leaf_id, bdd::node_id_t id,
                          std::optional<bool> direction, bool accepted) {
    assert(initial_state);
    assert(states.find(leaf_id) != states.end());

    ParserState *leaf = states[leaf_id];

    switch (leaf->type) {
    case ParserStateType::EXTRACT: {
      assert(!direction.has_value());
      ParserStateExtract *extractor = static_cast<ParserStateExtract *>(leaf);

      if (!extractor->next ||
          extractor->next->type != ParserStateType::TERMINATE) {
        return false;
      }

      ParserStateTerminate *terminate =
          static_cast<ParserStateTerminate *>(extractor->next);
      assert(terminate->accept == accepted);
    } break;
    case ParserStateType::SELECT: {
      assert(direction.has_value());
      ParserStateSelect *condition = static_cast<ParserStateSelect *>(leaf);

      if ((*direction && !condition->on_true) ||
          (!*direction && !condition->on_false)) {
        return false;
      }

      ParserState *next = *direction ? condition->on_true : condition->on_false;

      if (!next || next->type != ParserStateType::TERMINATE) {
        return false;
      }

      ParserStateTerminate *terminate =
          static_cast<ParserStateTerminate *>(next);
      assert(terminate->accept == accepted);
    } break;
    case ParserStateType::TERMINATE: {
      ParserStateTerminate *terminate =
          static_cast<ParserStateTerminate *>(leaf);
      assert(terminate->accept == accepted);
    } break;
    }

    return true;
  }

  void add_state(ParserState *new_state) {
    assert(!initial_state);
    assert(states.empty());

    initial_state = new_state;
    states[new_state->id] = new_state;

    // std::cerr << "-> NEW PARSER STATE <-\n";
    // std::cerr << dump() << std::endl;
  }

  void add_state(bdd::node_id_t leaf_id, ParserState *new_state,
                 std::optional<bool> direction) {
    assert(initial_state);
    assert(states.find(leaf_id) != states.end());
    assert(states.find(new_state->id) == states.end());

    states[new_state->id] = new_state;

    ParserState *leaf = states[leaf_id];

    switch (leaf->type) {
    case ParserStateType::EXTRACT: {
      assert(!direction.has_value());
      ParserStateExtract *extractor = static_cast<ParserStateExtract *>(leaf);
      assert(!extractor->next);
      extractor->next = new_state;
    } break;
    case ParserStateType::SELECT: {
      assert(direction.has_value());
      ParserStateSelect *condition = static_cast<ParserStateSelect *>(leaf);
      if (*direction) {
        assert(!condition->on_true);
        condition->on_true = new_state;
      } else {
        assert(!condition->on_false);
        condition->on_false = new_state;
      }
    } break;
    case ParserStateType::TERMINATE: {
      assert(false && "Cannot add state to terminating state");
    } break;
    }

    // std::cerr << "-> NEW PARSER STATE <-\n";
    // std::cerr << dump() << std::endl;
  }
};

} // namespace tofino
} // namespace synapse