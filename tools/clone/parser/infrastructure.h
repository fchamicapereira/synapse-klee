  #pragma once

  #include <list>
  #include <string>
  #include <map>

  #include "call-paths-to-bdd.h"

  #include "../models/device.h"
  #include "../models/link.h"
  #include "../models/port.h"

  #include "../util/logger.h"


  namespace Clone {
  using kutil::solver_toolbox;
  using std::string;
  using std::list;
  using std::map;

  struct Graph;
  typedef shared_ptr<Graph> GraphPtr;

  class Infrastructure {
  protected:
    const DeviceMap devices;
    const LinkList links;
    const PortMap ports;
    GraphPtr graph;

    /*	All strings are device ids
      < source < destination, next > > */ 
    map<string, map<string, string>> m_routing_table; 

    Infrastructure(DeviceMap&& devices, LinkList&& links, PortMap&& ports);

    map<string, string> dijkstra(DevicePtr device);
  public:
    static unique_ptr<Infrastructure> create(DeviceMap&& devices, LinkList&& links, PortMap&& ports);

    static unsigned extract_port(const BDD::Branch* branch) {
      auto kind = branch->get_condition()->getKind();
      assert(kind == klee::Expr::Kind::Eq);

      auto condition = branch->get_condition();
      auto right = condition->getKid(1);
      assert(right->getKind() == klee::Expr::Kind::Constant);
      auto casted = static_cast<klee::ConstantExpr*>(right.get());

      return casted->getZExtValue();
    }

    static void replace_branch_port(BDD::Node_ptr node, unsigned port) {
      BDD::Branch* branch = static_cast<BDD::Branch*>(node.get());
      auto condition = branch->get_condition();
      auto left = condition->getKid(0);
      auto right = condition->getKid(1);
      assert(right->getKind() == klee::Expr::Kind::Constant);

      auto new_right = klee::ConstantExpr::create(port, 32);
      auto new_condition = klee::EqExpr::create(left, new_right);
      branch->set_condition(new_condition);
    }

    static BDD::Node_ptr get_if_vigor_device(unsigned input_port, BDD::node_id_t& id) {
      auto vigor_device = solver_toolbox.create_new_symbol("VIGOR_DEVICE", 32);
      auto port = solver_toolbox.exprBuilder->Constant(input_port, vigor_device->getWidth()) ;
      auto eq = solver_toolbox.exprBuilder->Eq(vigor_device, port) ;
      BDD::Node_ptr node = BDD::Node_ptr(new BDD::Branch(id++, {}, eq));
      BDD::Node_ptr return_drop = get_drop(node, id++, 0);
      BDD::Node_ptr return_fwd = get_drop(node, id++, -1);
      BDD::Branch* node_casted = static_cast<BDD::Branch*>(node.get());
      node_casted->add_on_false(return_drop);
      node_casted->add_on_true(return_fwd);

      return node;
    }

    static void trim_node(BDD::Node_ptr curr, BDD::Node_ptr next) {
      auto prev = curr->get_prev();

      if(prev->get_type() == BDD::Node::NodeType::BRANCH) {
        auto branch { static_cast<BDD::Branch*>(prev.get()) };
        auto on_true = branch->get_on_true();
        auto on_false = branch->get_on_false();

        if (on_true->get_id() == curr->get_id()) {
          branch->replace_on_true(next);
          next->replace_prev(prev);
          assert(branch->get_on_true()->get_prev()->get_id() == prev->get_id());
        }
        else if (on_false->get_id() == curr->get_id()) {
          branch->replace_on_false(next);
          next->replace_prev(prev);
          assert(branch->get_on_false()->get_prev()->get_id() == prev->get_id());
        }
        else {
          danger("Could not trim branch ", prev->get_id(), " to leaf ", curr->get_id());
        }
      }
      else {
        prev->replace_next(next);
        next->replace_prev(prev);
      }
    }

    // get a ReturnProcess::Drop with prev as previous
    static BDD::Node_ptr get_drop(BDD::Node_ptr prev, BDD::node_id_t id, int value = 0) {
      return BDD::Node_ptr(new BDD::ReturnProcess(id, prev, {}, value, BDD::ReturnProcess::Operation::DROP));
    }

    const list<string> get_device_types() const;

    inline PortPtr get_port(unsigned port) const {
      return ports.count(port) ? ports.at(port) : nullptr;
    }

    inline const PortMap& get_ports() const {
      return ports;
    }

    inline const DeviceMap& get_devices() const {
      return devices;
    }

    inline DevicePtr get_device(const string& name) const {
      assert(devices.count(name));
      return devices.at(name);
    }

    void build_routing_table();

    // get routing table for device source
    inline const map<string, string>& get_routing_table(const string& source) const {
      return m_routing_table.at(source);
    }

    unsigned get_local_port(BDD::Node_ptr node) const {
      assert(node->get_type() == BDD::Node::NodeType::BRANCH);
      BDD::Branch* casted = static_cast<BDD::Branch*>(node.get());

      unsigned value = extract_port(casted);
      auto port = get_port(value);
      unsigned local_port = port->get_local_port();

      return local_port;
    }

    void print() const;
  };
}
