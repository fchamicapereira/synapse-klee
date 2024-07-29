#pragma once

#include "klee-util.h"
#include "call-paths-to-bdd.h"

#include <vector>
#include <string>
#include <sstream>
#include <optional>

#include "../../../log.h"

namespace synapse {
namespace tofino {

struct ParserState;
typedef std::unordered_map<bdd::node_id_t, ParserState *> states_t;

enum class ParserStateType { EXTRACT, SELECT, TERMINATE };

struct ParserState {
  bdd::nodes_t ids;
  ParserStateType type;

  ParserState(bdd::node_id_t _id, ParserStateType _type)
      : ids({_id}), type(_type) {}

  ParserState(const bdd::nodes_t &_ids, ParserStateType _type)
      : ids(_ids), type(_type) {}

  virtual ~ParserState() {}

  virtual std::string dump(int lvl = 0) const {
    std::stringstream ss;

    ss << std::string(lvl * 2, ' ');
    ss << "[";
    bool first = true;
    for (bdd::node_id_t id : ids) {
      if (first) {
        first = false;
      } else {
        ss << ", ";
      }
      ss << id;
    }
    ss << "] ";

    return ss.str();
  }

  virtual ParserState *clone() const = 0;

  virtual bool equals(const ParserState *other) const {
    if (!other || other->type != type) {
      return false;
    }

    return true;
  }

  virtual void record(states_t &states) {
    for (bdd::node_id_t id : ids) {
      states[id] = this;
    }
  }
};

struct ParserStateTerminate : public ParserState {
  bool accept;

  ParserStateTerminate(bdd::node_id_t _id, bool _accept)
      : ParserState(_id, ParserStateType::TERMINATE), accept(_accept) {}

  std::string dump(int lvl = 0) const override {
    std::stringstream ss;
    ss << ParserState::dump(lvl);
    ss << (accept ? "ACCEPT" : "REJECT");
    ss << "\n";
    return ss.str();
  }

  ParserState *clone() const { return new ParserStateTerminate(*this); }

  bool equals(const ParserState *other) const override {
    if (!ParserState::equals(other)) {
      return false;
    }

    const ParserStateTerminate *other_terminate =
        static_cast<const ParserStateTerminate *>(other);
    return other_terminate->accept == accept;
  }
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

  std::string dump(int lvl = 0) const override {
    std::stringstream ss;

    ss << ParserState::dump(lvl);
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
    ParserStateSelect *clone = new ParserStateSelect(*this);
    clone->on_true = on_true ? on_true->clone() : nullptr;
    clone->on_false = on_false ? on_false->clone() : nullptr;
    return clone;
  }

  bool equals(const ParserState *other) const override {
    if (!ParserState::equals(other)) {
      return false;
    }

    const ParserStateSelect *other_select =
        static_cast<const ParserStateSelect *>(other);

    if (!kutil::solver_toolbox.are_exprs_always_equal(field,
                                                      other_select->field)) {
      return false;
    }

    if (values.size() != other_select->values.size()) {
      return false;
    }

    for (size_t i = 0; i < values.size(); i++) {
      if (values[i] != other_select->values[i]) {
        return false;
      }
    }

    return true;
  }

  void record(states_t &states) override {
    ParserState::record(states);
    if (on_true)
      on_true->record(states);
    if (on_false)
      on_false->record(states);
  }
};

struct ParserStateExtract : public ParserState {
  klee::ref<klee::Expr> hdr;
  ParserState *next;

  ParserStateExtract(bdd::node_id_t _id, klee::ref<klee::Expr> _hdr)
      : ParserState(_id, ParserStateType::EXTRACT), hdr(_hdr), next(nullptr) {}

  std::string dump(int lvl = 0) const override {
    std::stringstream ss;

    ss << ParserState::dump(lvl);
    ss << "extract(" << kutil::expr_to_string(hdr, true) << ")\n";

    lvl++;

    if (next) {
      ss << next->dump(lvl + 1);
    }

    return ss.str();
  }

  ParserState *clone() const {
    ParserStateExtract *clone = new ParserStateExtract(*this);
    clone->next = next ? next->clone() : nullptr;
    return clone;
  }

  bool equals(const ParserState *other) const override {
    if (!ParserState::equals(other)) {
      return false;
    }

    const ParserStateExtract *other_extract =
        static_cast<const ParserStateExtract *>(other);

    return kutil::solver_toolbox.are_exprs_always_equal(hdr,
                                                        other_extract->hdr);
  }

  void record(states_t &states) override {
    ParserState::record(states);
    if (next)
      next->record(states);
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
      initial_state->record(states);
    }
  }

  ~Parser() {
    bdd::nodes_t freed;

    // The states data structure can have duplicates, so we need to make sure
    for (const auto &[node_id, state] : states) {
      if (freed.find(node_id) == freed.end()) {
        freed.insert(state->ids.begin(), state->ids.end());
        delete state;
      }
    }
  }

  const ParserState *get_initial_state() const { return initial_state; }

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

  void accept(bdd::node_id_t id) {
    if (already_terminated(id, true)) {
      return;
    }

    ParserState *new_state = new ParserStateTerminate(id, true);
    add_state(new_state);
  }

  void reject(bdd::node_id_t id) {
    if (already_terminated(id, false)) {
      return;
    }

    ParserState *new_state = new ParserStateTerminate(id, false);
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

  void log_debug() const {
    Log::dbg() << "******  Parser ******\n";
    if (initial_state)
      Log::dbg() << initial_state->dump();
    Log::dbg() << "************************\n";
  }

private:
  bool already_terminated(bdd::node_id_t id, bool accepted) {
    if (!initial_state) {
      return false;
    }

    assert(initial_state->type == ParserStateType::TERMINATE);
    ParserStateTerminate *terminate =
        static_cast<ParserStateTerminate *>(initial_state);
    assert(terminate->accept == accepted);

    return true;
  }

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
    assert(!new_state->ids.empty());

    initial_state = new_state;
    states[*new_state->ids.begin()] = new_state;
  }

  void set_next(ParserState *&next_state, ParserState *new_state) {
    if (next_state && next_state->equals(new_state)) {
      assert(new_state->ids.size() == 1);

      bdd::node_id_t new_id = *new_state->ids.begin();
      assert(next_state->ids.find(new_id) == next_state->ids.end());

      next_state->ids.insert(new_id);

      // Fix the incorrect previous recording
      states[new_id] = next_state;
      delete new_state;

      return;
    }

    ParserState *old_next_state = next_state;
    next_state = new_state;

    if (!old_next_state) {
      return;
    }

    assert(old_next_state->type == ParserStateType::TERMINATE);
    ParserStateTerminate *terminate =
        static_cast<ParserStateTerminate *>(old_next_state);
    assert(terminate->accept == true);

    switch (new_state->type) {
    case ParserStateType::EXTRACT: {
      ParserStateExtract *extractor =
          static_cast<ParserStateExtract *>(new_state);
      assert(!extractor->next);
      extractor->next = old_next_state;
    } break;
    case ParserStateType::SELECT: {
      ParserStateSelect *condition =
          static_cast<ParserStateSelect *>(new_state);
      assert(!condition->on_true);
      assert(!condition->on_false);
      condition->on_true = next_state;
      condition->on_false = next_state;
    } break;
    case ParserStateType::TERMINATE: {
      assert(false && "Cannot add state to terminating state");
    } break;
    }
  }

  void add_state(bdd::node_id_t leaf_id, ParserState *new_state,
                 std::optional<bool> direction) {
    assert(initial_state);
    assert(states.find(leaf_id) != states.end());
    assert(!new_state->ids.empty());
    for (bdd::node_id_t id : new_state->ids) {
      assert(states.find(id) == states.end());
    }

    states[*new_state->ids.begin()] = new_state;

    ParserState *leaf = states[leaf_id];

    switch (leaf->type) {
    case ParserStateType::EXTRACT: {
      assert(!direction.has_value());
      ParserStateExtract *extractor = static_cast<ParserStateExtract *>(leaf);
      set_next(extractor->next, new_state);
    } break;
    case ParserStateType::SELECT: {
      assert(direction.has_value());
      ParserStateSelect *condition = static_cast<ParserStateSelect *>(leaf);
      if (*direction) {
        set_next(condition->on_true, new_state);
      } else {
        set_next(condition->on_false, new_state);
      }
    } break;
    case ParserStateType::TERMINATE: {
      assert(false && "Cannot add state to terminating state");
    } break;
    }
  }
};

} // namespace tofino
} // namespace synapse