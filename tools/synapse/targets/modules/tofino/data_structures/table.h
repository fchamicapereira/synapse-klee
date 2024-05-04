#pragma once

#include <unordered_set>
#include <vector>

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "data_structure.h"

namespace synapse {
namespace targets {
namespace tofino {

class Table;
typedef std::shared_ptr<Table> TableRef;

class Table : public DataStructure {
public:
  struct key_t {
    klee::ref<klee::Expr> expr;
    std::vector<meta_t> meta;

    key_t(klee::ref<klee::Expr> _expr) : expr(_expr) {}
    key_t(klee::ref<klee::Expr> _expr, std::vector<meta_t> _meta)
        : expr(_expr), meta(_meta) {}
  };

  struct param_t {
    std::unordered_set<addr_t> objs;
    std::vector<klee::ref<klee::Expr>> exprs;

    param_t(addr_t obj, klee::ref<klee::Expr> expr) {
      objs.insert(obj);
      exprs.push_back(expr);
    }
  };

protected:
  std::string name;
  std::vector<key_t> keys;
  std::vector<param_t> params;
  std::vector<BDD::symbol_t> hit;

  bool manage_expirations;
  time_ms_t timeout;

public:
  Table(const Table &table)
      : DataStructure(Type::TABLE, table.objs, table.nodes), name(table.name),
        keys(table.keys), params(table.params), hit(table.hit),
        manage_expirations(table.manage_expirations), timeout(table.timeout) {}

  Table(const std::string &_name, const std::vector<key_t> &_keys,
        const std::vector<param_t> &_params,
        const std::vector<BDD::symbol_t> &_hit, addr_t _obj,
        const std::unordered_set<BDD::node_id_t> &_nodes)
      : DataStructure(Type::TABLE, {_obj}, _nodes), name(_name), keys(_keys),
        params(_params), hit(_hit), manage_expirations(false) {}

  void change_keys(const std::vector<key_t> &_new_keys) { keys = _new_keys; }

  void add_hit(const std::vector<BDD::symbol_t> &other_hit) {
    hit.insert(hit.end(), other_hit.begin(), other_hit.end());
  }

  void should_manage_expirations(time_ns_t _timeout) {
    manage_expirations = true;
    timeout = _timeout / 1000000; // ns -> ms
  }

  bool is_managing_expirations() const { return manage_expirations; }

  bool operator==(const Table &other) const { return name == other.name; }

  const std::string &get_name() const { return name; }
  const std::vector<key_t> &get_keys() const { return keys; }
  const std::vector<param_t> &get_params() const { return params; }
  const std::vector<BDD::symbol_t> &get_hit() const { return hit; }

  static TableRef build(const std::string &_base_name,
                        const std::vector<key_t> &_keys,
                        const std::vector<param_t> &_params,
                        const std::vector<BDD::symbol_t> &_hit, addr_t _obj,
                        const std::unordered_set<BDD::node_id_t> &_nodes) {
    auto _name = build_name(_base_name, _nodes);
    return TableRef(new Table(_name, _keys, _params, _hit, _obj, _nodes));
  }

  static TableRef
  build_for_obj(const std::string &_base_name, const std::vector<key_t> &_keys,
                const std::vector<param_t> &_params,
                const std::vector<BDD::symbol_t> &_hit, addr_t _obj,
                const std::unordered_set<BDD::node_id_t> &_nodes) {
    auto _name = build_name(_base_name, _obj);
    return TableRef(new Table(_name, _keys, _params, _hit, _obj, _nodes));
  }

  TableRef clone() { return TableRef(new Table(*this)); }

  bool equals(const Table *other) const {
    if (!DataStructure::equals(other)) {
      return false;
    }

    if (name != other->get_name()) {
      return false;
    }

    return true;
  }

  bool equals(const DataStructure *other) const override {
    if (!DataStructure::equals(other)) {
      return false;
    }

    auto other_casted = static_cast<const Table *>(other);
    return equals(other_casted);
  }

  std::ostream &dump(std::ostream &os) const {
    os << "table: " << name << "\n";

    os << "nodes: [";
    for (auto node : nodes) {
      os << node << ",";
    }
    os << "]\n";

    os << "objs: [";
    for (auto obj : objs) {
      os << obj << ",";
    }
    os << "]\n";

    os << "keys:\n";
    for (auto key : keys) {
      os << "  meta: [";
      for (auto meta : key.meta) {
        os << meta.symbol << ",";
      }
      os << "]\n";
      os << "  expr: " << kutil::pretty_print_expr(key.expr) << "\n";
    }

    os << "params:\n";
    for (auto param : params) {
      os << "  objs: ";
      for (auto obj : param.objs) {
        os << obj << ", ";
      }
      os << "\n";
      os << "  exprs: ";
      for (auto expr : param.exprs) {
        os << kutil::pretty_print_expr(expr) << ", ";
      }
      os << "\n";
    }

    os << "hit: [";
    for (auto symbol : hit) {
      os << symbol.label << ",";
    }
    os << "]\n";

    if (manage_expirations) {
      os << "timeout: " << timeout << " ms\n";
    }

    return os;
  }

protected:
  static std::string
  build_name(const std::string &base_name,
             const std::unordered_set<BDD::node_id_t> &nodes) {
    std::stringstream table_label_builder;

    table_label_builder << base_name;

    for (auto node_id : nodes) {
      table_label_builder << "_";
      table_label_builder << node_id;
    }

    return table_label_builder.str();
  }

  static std::string build_name(const std::string &base_name, addr_t obj) {
    std::stringstream table_label_builder;

    table_label_builder << base_name;
    table_label_builder << "_";
    table_label_builder << obj;

    return table_label_builder.str();
  }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
