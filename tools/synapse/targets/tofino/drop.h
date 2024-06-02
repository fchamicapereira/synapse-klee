#pragma once

#include "tofino_module.h"

#include "else.h"
#include "then.h"

#include "../../visualizers/ep_visualizer.h"

namespace synapse {
namespace tofino {

class Drop : public TofinoModule {
private:
  bool parser_reject;

public:
  Drop(const bdd::Node *node, bool _parser_reject)
      : TofinoModule(ModuleType::Tofino_Drop, "Drop", node),
        parser_reject(_parser_reject) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Drop *cloned = new Drop(node, parser_reject);
    return cloned;
  }

  bool is_parser_reject() const { return parser_reject; }
};

class DropGenerator : public TofinoModuleGenerator {
public:
  DropGenerator() : TofinoModuleGenerator(ModuleType::Tofino_Drop, "Drop") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::ROUTE) {
      return new_eps;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    if (op != bdd::RouteOperation::DROP) {
      return new_eps;
    }

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    bool parser_reject = is_parser_reject(ep);
    Module *module = new Drop(node, parser_reject);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);
    if (parser_reject) {
      tofino_ctx->parser_reject(ep, node);
    } else {
      tofino_ctx->parser_accept(ep, node);
    }

    return new_eps;
  }

private:
  bool is_parser_reject(const EP *ep) const {
    const EPLeaf *leaf = ep->get_active_leaf();

    if (!leaf || !leaf->node || !leaf->node->get_prev()) {
      return false;
    }

    const EPNode *node = leaf->node;
    const EPNode *prev = node->get_prev();
    const Module *module = prev->get_module();

    ModuleType type = module->get_type();
    return (type == ModuleType::Tofino_ParserCondition);
  }
};

} // namespace tofino
} // namespace synapse
