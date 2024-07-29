#include "ep_visualizer.h"

#include "../log.h"
#include "../execution_plan/execution_plan.h"
#include "../targets/targets.h"

#include <ctime>
#include <fstream>
#include <limits>
#include <math.h>
#include <unistd.h>

#define SHOW_MODULE_NAME(M)                                                    \
  void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,                \
                           const M *node) {                                    \
    function_call(ep_node, node->get_node(), node->get_target(),               \
                  node->get_name());                                           \
  }

#define VISIT_BRANCH(M)                                                        \
  void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,                \
                           const M *node) {                                    \
    branch(ep_node, node->get_node(), node->get_target(), node->get_name());   \
  }

#define IGNORE_MODULE(M)                                                       \
  void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,                \
                           const M *node) {}

namespace synapse {

using namespace tofino;

IGNORE_MODULE(tofino::Ignore)

VISIT_BRANCH(tofino::IfSimple)
VISIT_BRANCH(tofino::If)
VISIT_BRANCH(tofino::ParserCondition)

SHOW_MODULE_NAME(tofino::ParserReject)
SHOW_MODULE_NAME(tofino::SendToController)
SHOW_MODULE_NAME(tofino::Drop)
SHOW_MODULE_NAME(tofino::Broadcast)
SHOW_MODULE_NAME(tofino::ModifyHeader)
SHOW_MODULE_NAME(tofino::Then)
SHOW_MODULE_NAME(tofino::Else)

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::Recirculate *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  int port = node->get_recirc_port();

  label_builder << "Recirculate (";
  label_builder << port;
  label_builder << ")";

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::Forward *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  int dst_device = node->get_dst_device();

  label_builder << "Forward (";
  label_builder << dst_device;
  label_builder << ")";

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::SimpleTableLookup *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  DS_ID tid = node->get_table_id();
  addr_t obj = node->get_obj();

  label_builder << "SimpleTable Lookup\n";
  label_builder << "(";
  label_builder << "tid=";
  label_builder << tid;
  label_builder << ", obj=";
  label_builder << obj;
  label_builder << ")";

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::VectorRegisterLookup *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  const std::unordered_set<DS_ID> &rids = node->get_rids();
  addr_t obj = node->get_obj();

  label_builder << "Register Lookup\n";
  label_builder << "(";
  label_builder << "rids=[";

  int i = 0;
  for (const std::string &rid : rids) {
    label_builder << rid;
    i++;
    if (i < (int)rids.size()) {
      label_builder << ",";
      if (i % 3 == 0) {
        label_builder << "\\n";
      }
    }
  }

  label_builder << "], obj=";
  label_builder << obj;
  label_builder << ")";

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::VectorRegisterUpdate *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  const std::unordered_set<DS_ID> &rids = node->get_rids();
  addr_t obj = node->get_obj();

  label_builder << "Register Update\n";

  label_builder << "rids=[";

  int i = 0;
  for (const std::string &rid : rids) {
    label_builder << rid;
    i++;
    if (i < (int)rids.size()) {
      label_builder << ",";
      if (i % 3 == 0) {
        label_builder << "\\n";
      }
    }
  }

  label_builder << "], obj=";
  label_builder << obj;

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::TTLCachedTableRead *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  addr_t obj = node->get_obj();

  tofino::DS_ID id = node->get_id();
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();
  const DS *ds = tofino_ctx->get_ds_from_id(id);
  assert(ds->type == DSType::CACHED_TABLE);
  const TTLCachedTable *cached_table = static_cast<const TTLCachedTable *>(ds);

  label_builder << "TTL Cached Table Read\n";

  label_builder << "obj=";
  label_builder << obj;
  label_builder << ", ";
  label_builder << "size=";
  label_builder << cached_table->cache_capacity;

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::TTLCachedTableReadOrWrite *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  addr_t obj = node->get_obj();

  tofino::DS_ID id = node->get_id();
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();
  const DS *ds = tofino_ctx->get_ds_from_id(id);
  assert(ds->type == DSType::CACHED_TABLE);
  const TTLCachedTable *cached_table = static_cast<const TTLCachedTable *>(ds);

  label_builder << "TTL Cached Table Read/Write\n";

  label_builder << "obj=";
  label_builder << obj;
  label_builder << ", ";
  label_builder << "size=";
  label_builder << cached_table->cache_capacity;

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::TTLCachedTableWrite *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  addr_t obj = node->get_obj();

  tofino::DS_ID id = node->get_id();
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();
  const DS *ds = tofino_ctx->get_ds_from_id(id);
  assert(ds->type == DSType::CACHED_TABLE);
  const TTLCachedTable *cached_table = static_cast<const TTLCachedTable *>(ds);

  label_builder << "TTL Cached Table Write\n";

  label_builder << "obj=";
  label_builder << obj;
  label_builder << ", ";
  label_builder << "size=";
  label_builder << cached_table->cache_capacity;

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::TTLCachedTableConditionalDelete *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  addr_t obj = node->get_obj();

  tofino::DS_ID id = node->get_id();
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();
  const DS *ds = tofino_ctx->get_ds_from_id(id);
  assert(ds->type == DSType::CACHED_TABLE);
  const TTLCachedTable *cached_table = static_cast<const TTLCachedTable *>(ds);

  label_builder << "TTL Cached Table Conditional Delete\n";

  label_builder << "obj=";
  label_builder << obj;
  label_builder << ", ";
  label_builder << "size=";
  label_builder << cached_table->cache_capacity;

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::TTLCachedTableDelete *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  addr_t obj = node->get_obj();

  tofino::DS_ID id = node->get_id();
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();
  const DS *ds = tofino_ctx->get_ds_from_id(id);
  assert(ds->type == DSType::CACHED_TABLE);
  const TTLCachedTable *cached_table = static_cast<const TTLCachedTable *>(ds);

  label_builder << "TTL Cached Table Delete\n";

  label_builder << "obj=";
  label_builder << obj;
  label_builder << ", ";
  label_builder << "size=";
  label_builder << cached_table->cache_capacity;

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

void EPVisualizer::visit(const EP *ep, const EPNode *ep_node,
                         const tofino::ParserExtraction *node) {
  std::stringstream label_builder;

  const bdd::Node *bdd_node = node->get_node();
  TargetType target = node->get_target();
  bytes_t size = node->get_length();

  label_builder << "Parse Header (";

  label_builder << size;
  label_builder << "B)";

  std::string label = label_builder.str();
  function_call(ep_node, bdd_node, target, label);
}

} // namespace synapse