#include "synthesizer.h"
#include "../../targets/module.h"

namespace synapse {
namespace tofino {

const std::unordered_set<ModuleType> branching_modules = {
    ModuleType::Tofino_If,
    ModuleType::Tofino_IfSimple,
};

static bool is_branching_node(const EPNode *node) {
  const Module *module = node->get_module();
  ModuleType type = module->get_type();
  return branching_modules.find(type) != branching_modules.end();
}

static const DS *get_tofino_ds(const EP *ep, DS_ID id) {
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();
  const DS *ds = tofino_ctx->get_ds_from_id(id);
  assert(ds && "DS not found");
  return ds;
}

TofinoSynthesizer::TofinoSynthesizer(const std::filesystem::path &out_dir,
                                     const bdd::BDD *bdd)
    : Synthesizer(TEMPLATE_FILENAME,
                  {
                      {MARKER_CPU_HEADER, 1},
                      {MARKER_CUSTOM_HEADERS, 0},
                      {MARKER_INGRESS_HEADERS, 1},
                      {MARKER_INGRESS_METADATA, 1},
                      {MARKER_INGRESS_PARSER, 1},
                      {MARKER_INGRESS_CONTROL, 1},
                      {MARKER_INGRESS_CONTROL_APPLY, 2},
                      {MARKER_INGRESS_DEPARSER, 2},
                      {MARKER_EGRESS_HEADERS, 1},
                      {MARKER_EGRESS_METADATA, 1},
                  },
                  out_dir / OUTPUT_FILENAME),
      var_stacks(1), transpiler(this) {
  symbol_t device = bdd->get_device();
  symbol_t time = bdd->get_time();

  // Hack
  var_stacks.back().emplace_back(
      "ig_intr_md.ingress_port",
      kutil::solver_toolbox.exprBuilder->Extract(device.expr, 0, 16));

  var_stacks.back().emplace_back("ig_intr_md.ingress_mac_tstamp[47:16]", time);
}

void TofinoSynthesizer::visit(const EP *ep) {
  EPVisitor::visit(ep);
  Synthesizer::dump();
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *node) {
  EPVisitor::visit(ep, node);

  if (is_branching_node(node)) {
    return;
  }

  const std::vector<EPNode *> &children = node->get_children();
  for (const EPNode *child : children) {
    visit(ep, child);
  }
}

code_t TofinoSynthesizer::slice_var(const var_t &var, unsigned offset,
                                    bits_t size) const {
  assert(offset + size <= var.expr->getWidth());

  code_builder_t builder;
  builder << var.name << "[" << offset + size - 1 << ":" << offset << "]";
  return builder.dump();
}

code_t TofinoSynthesizer::type_from_expr(klee::ref<klee::Expr> expr) const {
  klee::Expr::Width width = expr->getWidth();
  assert(width != klee::Expr::InvalidWidth);
  code_builder_t builder;
  builder << "bit<" << width << ">";
  return builder.dump();
}

bool TofinoSynthesizer::get_var(klee::ref<klee::Expr> expr,
                                code_t &out_var) const {
  for (auto it = var_stacks.rbegin(); it != var_stacks.rend(); ++it) {
    const vars_t &vars = *it;
    for (const var_t &var : vars) {
      if (kutil::solver_toolbox.are_exprs_always_equal(var.expr, expr)) {
        out_var = var.name;
        return true;
      }

      klee::Expr::Width expr_bits = expr->getWidth();
      klee::Expr::Width var_bits = var.expr->getWidth();

      if (expr_bits > var_bits) {
        continue;
      }

      for (bits_t offset = 0; offset <= var_bits - expr_bits; offset += 8) {
        klee::ref<klee::Expr> var_slice =
            kutil::solver_toolbox.exprBuilder->Extract(var.expr, offset,
                                                       expr_bits);

        if (kutil::solver_toolbox.are_exprs_always_equal(var_slice, expr)) {
          out_var = slice_var(var, offset, expr_bits);
          return true;
        }
      }
    }
  }

  return false;
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::SendToController *node) {
  code_builder_t &ingress = get(MARKER_INGRESS_CONTROL_APPLY);
  ingress.indent();
  ingress << "send_to_controller();\n";
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::Recirculate *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::Ignore *node) {}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::If *node) {
  code_builder_t &ingress = get(MARKER_INGRESS_CONTROL_APPLY);

  const std::vector<klee::ref<klee::Expr>> &conditions = node->get_conditions();

  const std::vector<EPNode *> &children = ep_node->get_children();
  assert(children.size() == 2);

  const EPNode *then_node = children[0];
  const EPNode *else_node = children[1];

  code_t cond_val = get_unique_var_name("cond");

  ingress.indent();
  ingress << "bool " << cond_val << " = false;\n";

  for (klee::ref<klee::Expr> condition : conditions) {
    ingress.indent();
    ingress << "if (";
    ingress << transpiler.transpile(condition);
    ingress << ") {\n";

    ingress.inc();
  }

  ingress.indent();
  ingress << cond_val << " = true;\n";

  for (size_t i = 0; i < conditions.size(); i++) {
    ingress.dec();
    ingress.indent();
    ingress << "}\n";
  }

  ingress.indent();
  ingress << "if (";
  ingress << cond_val;
  // TODO: condition
  ingress << ") {\n";

  ingress.inc();
  var_stacks.emplace_back();
  visit(ep, then_node);
  var_stacks.pop_back();
  ingress.dec();

  ingress.indent();
  ingress << "} else {\n";

  ingress.inc();
  var_stacks.emplace_back();
  visit(ep, else_node);
  var_stacks.pop_back();
  ingress.dec();

  ingress.indent();
  ingress << "}\n";
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::IfSimple *node) {
  code_builder_t &ingress = get(MARKER_INGRESS_CONTROL_APPLY);

  klee::ref<klee::Expr> condition = node->get_condition();

  const std::vector<EPNode *> &children = ep_node->get_children();
  assert(children.size() == 2);

  const EPNode *then_node = children[0];
  const EPNode *else_node = children[1];

  ingress.indent();
  ingress << "if (";
  ingress << transpiler.transpile(condition);
  ingress << ") {\n";

  ingress.inc();
  var_stacks.emplace_back();
  visit(ep, then_node);
  var_stacks.pop_back();
  ingress.dec();

  ingress.indent();
  ingress.stream << "} else {\n";

  ingress.inc();
  var_stacks.emplace_back();
  visit(ep, else_node);
  var_stacks.pop_back();
  ingress.dec();

  ingress.indent();
  ingress << "}\n";
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::ParserCondition *node) {}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::Then *node) {}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::Else *node) {}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::Forward *node) {
  int dst_device = node->get_dst_device();

  code_builder_t &ingress = get(MARKER_INGRESS_CONTROL_APPLY);

  ingress.indent();
  ingress << "fwd(";
  ingress << dst_device;
  ingress << ");\n";
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::Drop *node) {
  code_builder_t &ingress = get(MARKER_INGRESS_CONTROL_APPLY);
  ingress.indent();
  ingress << "drop();\n";
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::Broadcast *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::ParserExtraction *node) {
  klee::ref<klee::Expr> hdr = node->get_hdr();
  code_t hdr_name = get_unique_var_name("hdr");

  code_builder_t &custom_hdrs = get(MARKER_CUSTOM_HEADERS);
  custom_hdrs.indent();
  custom_hdrs << "header " << hdr_name << "_h {\n";

  custom_hdrs.inc();
  custom_hdrs.indent();
  custom_hdrs << type_from_expr(hdr) << " data;\n";

  custom_hdrs.dec();
  custom_hdrs.indent();
  custom_hdrs << "}\n";

  code_builder_t &ingress_hdrs = get(MARKER_INGRESS_HEADERS);
  ingress_hdrs.indent();
  ingress_hdrs << hdr_name << "_h " << hdr_name << ";\n";

  var_stacks.back().emplace_back("hdr." + hdr_name + ".data", hdr);
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::ModifyHeader *node) {
  code_builder_t &ingress = get(MARKER_INGRESS_CONTROL_APPLY);

  const std::vector<modification_t> &changes = node->get_changes();
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::SimpleTableLookup *node) {
  code_builder_t &ingress = get(MARKER_INGRESS_CONTROL);
  code_builder_t &ingress_apply = get(MARKER_INGRESS_CONTROL_APPLY);

  DS_ID table_id = node->get_table_id();
  const std::vector<klee::ref<klee::Expr>> &keys = node->get_keys();
  const std::vector<klee::ref<klee::Expr>> &values = node->get_values();
  const std::optional<symbol_t> &hit = node->get_hit();

  const DS *ds = get_tofino_ds(ep, table_id);
  const Table *table = static_cast<const Table *>(ds);

  transpile_table(ingress, table, keys, values);

  ingress_apply.indent();

  if (hit) {
    code_t hit_var_name = "hit_" + table_id;
    ingress_apply << "bool " << hit_var_name << " = ";
    var_stacks.back().emplace_back(hit_var_name, hit.value());
  }

  ingress_apply << "table_" << table_id << "a.apply()";
  if (hit) {
    ingress_apply << ".hit";
  }
  ingress_apply << "\n";
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::VectorRegisterLookup *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::VectorRegisterUpdate *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::TTLCachedTableRead *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::TTLCachedTableReadOrWrite *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::TTLCachedTableWrite *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(
    const EP *ep, const EPNode *ep_node,
    const tofino::TTLCachedTableConditionalDelete *node) {
  // TODO:
  assert(false && "TODO");
}

void TofinoSynthesizer::visit(const EP *ep, const EPNode *ep_node,
                              const tofino::TTLCachedTableDelete *node) {
  // TODO:
  assert(false && "TODO");
}

code_t TofinoSynthesizer::get_unique_var_name(const code_t &prefix) {
  if (var_prefix_usage.find(prefix) == var_prefix_usage.end()) {
    var_prefix_usage[prefix] = 0;
  }

  int &counter = var_prefix_usage[prefix];

  code_builder_t builder;
  builder << prefix << "_" << counter;

  counter++;

  return builder.dump();
}

void TofinoSynthesizer::dbg_vars() const {
  Log::dbg() << "================= Vars ================= \n";
  for (const vars_t &vars : var_stacks) {
    Log::dbg() << "------------------------------------------\n";
    for (const var_t &var : vars) {
      Log::dbg() << var.name << ": ";
      Log::dbg() << kutil::expr_to_string(var.expr, false) << "\n";
    }
  }
  Log::dbg() << "======================================== \n";
}

} // namespace tofino
} // namespace synapse