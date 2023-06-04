#include "klee-util.h"

#include "../../../../log.h"
#include "../../../modules/modules.h"
#include "../util.h"
#include "tofino_generator.h"
#include "transpiler.h"
#include "util.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

std::string TofinoGenerator::transpile(klee::ref<klee::Expr> expr) {
  auto code = transpiler.transpile(expr);
  return code;
}

variable_query_t
TofinoGenerator::search_variable(const BDD::symbol_t &symbol) const {
  auto ingress_var = ingress.search_variable(symbol.label);

  if (ingress_var.valid) {
    return ingress_var;
  }

  if (symbol.expr.isNull()) {
    return variable_query_t();
  }

  auto res = kutil::get_symbol(symbol.expr);

  if (!res.first || res.second != symbol.label) {
    return variable_query_t();
  }

  return ingress.search_variable(symbol.expr);
}

variable_query_t
TofinoGenerator::search_variable(const std::string &symbol) const {
  auto ingress_var = ingress.search_variable(symbol);

  if (ingress_var.valid) {
    return ingress_var;
  }

  return variable_query_t();
}

variable_query_t
TofinoGenerator::search_variable(klee::ref<klee::Expr> expr) const {
  auto ingress_var = ingress.search_variable(expr);

  if (ingress_var.valid) {
    return ingress_var;
  }

  auto hdr_field = ingress.parser.headers.get_field(expr);

  if (hdr_field.valid) {
    return hdr_field;
  }

  if (kutil::is_readLSB(expr)) {
    auto symbol = kutil::get_symbol(expr);
    auto variable = search_variable(symbol.second);

    if (variable.valid) {
      return variable;
    }
  }

  return variable_query_t();
}

void TofinoGenerator::build_cpu_header(const ExecutionPlan &ep) {
  auto tmb = ep.get_memory_bank<target::TofinoMemoryBank>(Tofino);
  auto dp_state = tmb->get_dataplane_state();
  ingress.set_cpu_hdr_fields(dp_state);
}

void TofinoGenerator::allocate_table(const target::Table *_table) {
  assert(_table);

  auto table_name = _table->get_name();
  auto keys = _table->get_keys();
  auto params = _table->get_params();

  // TODO: get table size
  auto size = 1024;

  assert(keys.size());

  std::vector<std::vector<key_var_t>> keys_vars;

  for (auto key : keys) {
    auto kvs = get_key_vars(ingress, key.expr, key.meta);
    keys_vars.push_back(kvs);
  }

  Variables meta_params;

  for (auto i = 0u; i < params.size(); i++) {
    auto meta_param =
        ingress.allocate_meta_param(table_name, i, params[i].exprs);
    meta_params.push_back(meta_param);
  }

  table_t table(table_name, keys_vars, meta_params, size);
  ingress.add_table(table);
}

void TofinoGenerator::allocate_counter(const target::Counter *_counter) {
  assert(_counter);

  auto objs = _counter->get_objs();
  assert(objs.size() == 1);

  auto vector = *objs.begin();
  auto capacity = _counter->get_capacity();
  auto value_size = _counter->get_value_size();
  auto max_value = _counter->get_max_value();

  auto counter = counter_t(vector, capacity, value_size, max_value);
  ingress.add_counter(counter);
}

void TofinoGenerator::allocate_int_allocator(
    const target::IntegerAllocator *_int_allocator) {
  assert(_int_allocator);

  auto objs = _int_allocator->get_objs();
  assert(objs.size() == 1);

  auto dchain = *objs.begin();
  auto integer = _int_allocator->get_integer();
  auto out_of_space = _int_allocator->get_out_of_space();
  auto capacity = _int_allocator->get_capacity();
  auto integer_size = _int_allocator->get_integer_size();

  assert(integer.size() > 0);

  auto head_label = integer_allocator_t::get_head_label(dchain);
  auto tail_label = integer_allocator_t::get_tail_label(dchain);
  auto query_table_label = integer_allocator_t::get_query_table_label(dchain);
  auto rejuvenation_table_label =
      integer_allocator_t::get_rejuvenation_table_label(dchain);

  auto head_var = ingress.allocate_local_auxiliary(head_label, integer_size);
  auto tail_var = ingress.allocate_meta(tail_label, integer_size);

  std::vector<std::string> out_of_space_labels;
  for (auto oos : out_of_space) {
    out_of_space_labels.push_back(oos.label);
  }

  auto out_of_space_var = ingress.allocate_local_auxiliary(
      "out_of_space", 1, {}, out_of_space_labels);

  auto allocated_var = ingress.allocate_meta("allocated", integer);

  auto query_table =
      table_t(query_table_label, {allocated_var.get_label()}, {}, capacity);
  ingress.add_table(query_table);

  auto rejuvenation_table = table_t(rejuvenation_table_label,
                                    {allocated_var.get_label()}, {}, capacity);
  ingress.add_table(rejuvenation_table);

  auto int_allocator = integer_allocator_t(
      dchain, capacity, integer_size, head_var, tail_var, out_of_space_var,
      allocated_var, query_table, rejuvenation_table);
  ingress.add_integer_allocator(int_allocator);
}

void TofinoGenerator::allocate_state(const ExecutionPlan &ep) {
  auto tmb = ep.get_memory_bank<target::TofinoMemoryBank>(Tofino);
  auto implementations = tmb->get_implementations();

  for (const auto &impl : implementations) {
    switch (impl->get_type()) {
    case target::DataStructure::TABLE: {
      auto table = static_cast<target::Table *>(impl.get());
      allocate_table(table);
    } break;
    case target::DataStructure::INTEGER_ALLOCATOR: {
      auto int_allocator = static_cast<target::IntegerAllocator *>(impl.get());
      allocate_int_allocator(int_allocator);
    } break;
    case target::DataStructure::COUNTER: {
      auto counter = static_cast<target::Counter *>(impl.get());
      allocate_counter(counter);
    } break;
    }
  }
}

void TofinoGenerator::visit(ExecutionPlan ep) {
  // add expiration data
  auto mb = ep.get_memory_bank();
  auto expiration_data = mb->get_expiration_data();

  if (expiration_data.valid) {
    // Hack: replace it with a constant, as we don't care about it.
    // The Tofino compiler will optimize it away.
    auto expired_flows_expr = expiration_data.number_of_freed_flows.expr;
    auto expired_flows_label = expiration_data.number_of_freed_flows.label;

    auto var =
        Variable("0", expired_flows_expr->getWidth(), expired_flows_expr);
    ingress.local_vars.append(var);
  }

  build_cpu_header(ep);
  allocate_state(ep);

  ExecutionPlanVisitor::visit(ep);

  std::stringstream cpu_header_fields_code;
  std::stringstream ingress_headers_decl_code;
  std::stringstream ingress_headers_def_code;
  std::stringstream ingress_metadata_code;
  std::stringstream egress_metadata_code;
  std::stringstream ingress_parser_code;
  std::stringstream ingress_state_code;
  std::stringstream ingress_apply_code;

  ingress.synthesize_cpu_header(cpu_header_fields_code);
  ingress.synthesize_headers(ingress_headers_def_code,
                             ingress_headers_decl_code);
  ingress.synthesize_user_metadata(ingress_metadata_code);
  ingress.synthesize_parser(ingress_parser_code);
  ingress.synthesize_state(ingress_state_code);
  ingress.synthesize_apply_block(ingress_apply_code);

  fill_mark(MARKER_CPU_HEADER_FIELDS, cpu_header_fields_code.str());
  fill_mark(MARKER_INGRESS_HEADERS_DEF, ingress_headers_def_code.str());
  fill_mark(MARKER_INGRESS_HEADERS_DECL, ingress_headers_decl_code.str());
  fill_mark(MARKER_INGRESS_PARSER, ingress_parser_code.str());
  fill_mark(MARKER_INGRESS_METADATA, ingress_metadata_code.str());
  fill_mark(MARKER_EGRESS_METADATA, egress_metadata_code.str());
  fill_mark(MARKER_INGRESS_STATE, ingress_state_code.str());
  fill_mark(MARKER_INGRESS_APPLY, ingress_apply_code.str());
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node) {
  auto mod = ep_node->get_module();
  auto next = ep_node->get_next();

  log(ep_node);

  mod->visit(*this, ep_node);

  if (ep_node->is_terminal_node()) {
    auto closed = ingress.pending_ifs.close();
    for (auto i = 0; i < closed; i++) {
      ingress.local_vars.pop();
    }
  }

  for (auto branch : next) {
    branch->visit(*this);
  }
}

// Due to packet byte limitations (max 4 bytes per branching condition)
void TofinoGenerator::visit_if_multiple_conditions(
    std::vector<klee::ref<klee::Expr>> conditions) {
  auto cond = ingress.allocate_local_auxiliary("cond", 1);

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append(cond.get_type());
  ingress.apply_block_builder.append(" ");
  ingress.apply_block_builder.append(cond.get_label());
  ingress.apply_block_builder.append(" = false;");
  ingress.apply_block_builder.append_new_line();

  for (auto c : conditions) {
    auto transpiled = transpile(c);

    ingress.apply_block_builder.indent();
    ingress.apply_block_builder.append("if (");
    ingress.apply_block_builder.append(transpiled);
    ingress.apply_block_builder.append(") {");
    ingress.apply_block_builder.append_new_line();

    ingress.apply_block_builder.inc_indentation();
  }

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append(cond.get_label());
  ingress.apply_block_builder.append(" = true;");
  ingress.apply_block_builder.append_new_line();

  for (auto c : conditions) {
    ingress.apply_block_builder.dec_indentation();

    ingress.apply_block_builder.indent();
    ingress.apply_block_builder.append("}");
    ingress.apply_block_builder.append_new_line();
  }

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append("if (");
  ingress.apply_block_builder.append(cond.get_label());
  ingress.apply_block_builder.append(") {");
  ingress.apply_block_builder.append_new_line();

  ingress.apply_block_builder.inc_indentation();

  ingress.local_vars.push();
  ingress.pending_ifs.push();
}

void TofinoGenerator::visit_if_simple_condition(
    klee::ref<klee::Expr> condition) {
  auto transpiled = transpile(condition);

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append("if (");
  ingress.apply_block_builder.append(transpiled);
  ingress.apply_block_builder.append(") {");
  ingress.apply_block_builder.append_new_line();

  ingress.apply_block_builder.inc_indentation();

  ingress.local_vars.push();
  ingress.pending_ifs.push();
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::If *node) {
  assert(node);

  auto conditions = node->get_conditions();
  assert(conditions.size() > 0);

  if (conditions.size() > 1) {
    visit_if_multiple_conditions(conditions);
  } else {
    visit_if_simple_condition(conditions[0]);
  }
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::Then *node) {
  assert(node);
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::Else *node) {
  assert(node);

  ingress.local_vars.push();

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append("else {");
  ingress.apply_block_builder.append_new_line();
  ingress.apply_block_builder.inc_indentation();
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::Forward *node) {
  assert(node);
  auto port = node->get_port();

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append(INGRESS_FORWARD_ACTION);
  ingress.apply_block_builder.append("(");
  ingress.apply_block_builder.append(port);
  ingress.apply_block_builder.append(");");
  ingress.apply_block_builder.append_new_line();
}

bool has_pending_parsing_ops(const ExecutionPlanNode *node) {
  auto nodes = std::vector<const ExecutionPlanNode *>{node};

  while (nodes.size()) {
    auto node = nodes[0];
    nodes.erase(nodes.begin());

    assert(node);

    auto m = node->get_module();
    assert(m);

    if (m->get_type() == Module::Tofino_ParseCustomHeader ||
        m->get_type() == Module::Tofino_ParserCondition) {
      return true;
    }

    auto next = node->get_next();

    for (auto n : next) {
      nodes.push_back(n.get());
    }
  }

  return false;
}

bool get_expecting_parsing_operations_after_header_parse(
    const ExecutionPlanNode *ep_node) {
  auto next = ep_node->get_next();

  assert(next.size() == 1);
  return has_pending_parsing_ops(next[0].get());
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::ParseCustomHeader *node) {
  assert(node);

  auto chunk = node->get_chunk();
  auto size = node->get_size();
  auto pending = get_expecting_parsing_operations_after_header_parse(ep_node);

  ingress.parser.parse_header(chunk, size, pending);
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::ModifyCustomHeader *node) {
  assert(node);

  auto original_chunk = node->get_original_chunk();
  auto modifications = node->get_modifications();

  for (auto mod : modifications) {
    auto byte = mod.byte;
    auto expr = mod.expr;

    auto modified_byte = kutil::solver_toolbox.exprBuilder->Extract(
        original_chunk, byte * 8, klee::Expr::Int8);

    auto transpiled_byte = transpile(modified_byte);
    auto transpiled_expr = transpile(expr);

    ingress.apply_block_builder.indent();
    ingress.apply_block_builder.append(transpiled_byte);
    ingress.apply_block_builder.append(" = ");
    ingress.apply_block_builder.append(transpiled_expr);
    ingress.apply_block_builder.append(";");
    ingress.apply_block_builder.append_new_line();
  }
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::ParserCondition *node) {
  assert(node);

  auto condition = node->get_condition();
  auto reject_on_false = node->get_reject_on_false();
  auto apply_is_valid = node->get_apply_is_valid();
  auto expect_on_false = ep_node->get_next().size() == 2;

  auto data = transpiler.get_parser_cond_data(condition);

  ingress.parser.add_parsing_condition(data.hdr, data.values, expect_on_false,
                                       reject_on_false);

  if (apply_is_valid) {
    auto transpiled = transpile(condition);

    ingress.apply_block_builder.indent();
    ingress.apply_block_builder.append("if (");
    ingress.apply_block_builder.append(transpiled);
    ingress.apply_block_builder.append(") {");
    ingress.apply_block_builder.append_new_line();

    ingress.apply_block_builder.inc_indentation();

    ingress.local_vars.push();
    ingress.pending_ifs.push();
  }
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::IPv4TCPUDPChecksumsUpdate *node) {
  // TODO: implement
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::TableModule *node) {
  assert(node);

  auto _table = node->get_table();

  auto table_name = _table->get_name();
  auto keys = _table->get_keys();
  auto params = _table->get_params();
  auto hit = _table->get_hit();

  const auto &table = ingress.get_table(table_name);

  std::vector<std::vector<std::string>> assignments;

  assert(table.keys_vars.size() == keys.size());

  for (auto i = 0u; i < keys.size(); i++) {
    auto key_assignments =
        transpiler.assign_key_bytes(keys[i].expr, table.keys_vars[i]);
    assignments.push_back(key_assignments);
  }

  for (auto assignment : assignments[0]) {
    ingress.apply_block_builder.indent();
    ingress.apply_block_builder.append(assignment);
    ingress.apply_block_builder.append(";");
    ingress.apply_block_builder.append_new_line();
  }

  if (hit.size() > 0) {
    std::vector<std::string> symbols_labels;

    for (auto symbol : hit) {
      symbols_labels.push_back(symbol.label);
    }

    auto hit_var = ingress.allocate_local_auxiliary(table_name + "_hit", 1, {},
                                                    symbols_labels);
    table.synthesize_apply(ingress.apply_block_builder, hit_var);
  } else {
    table.synthesize_apply(ingress.apply_block_builder);
  }
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::TableLookup *node) {
  visit(ep_node, static_cast<const targets::tofino::TableModule *>(node));
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::TableIsAllocated *node) {
  visit(ep_node, static_cast<const targets::tofino::TableModule *>(node));
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::TableRejuvenation *node) {
  visit(ep_node, static_cast<const targets::tofino::TableModule *>(node));
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::IntegerAllocatorAllocate *node) {
  assert(node);

  auto _int_allocator = node->get_int_allocator();
  auto objs = _int_allocator->get_objs();
  assert(objs.size() == 1);

  auto dchain = *objs.begin();

  const auto &int_allocator = ingress.get_int_allocator(dchain);
  int_allocator.synthesize_allocate(ingress.apply_block_builder);
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::IntegerAllocatorRejuvenate *node) {
  assert(node);

  auto _int_allocator = node->get_int_allocator();
  auto index = node->get_index();
  auto objs = _int_allocator->get_objs();
  assert(objs.size() == 1);

  auto dchain = *objs.begin();

  const auto &int_allocator = ingress.get_int_allocator(dchain);
  auto allocated_meta = int_allocator.allocated;

  auto transpiled_integer = transpile(index);

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append(allocated_meta.get_label());
  ingress.apply_block_builder.append(" = ");
  ingress.apply_block_builder.append(transpiled_integer);
  ingress.apply_block_builder.append(";");
  ingress.apply_block_builder.append_new_line();

  int_allocator.synthesize_rejuvenate(ingress.apply_block_builder);
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::IntegerAllocatorQuery *node) {
  assert(node);

  auto _int_allocator = node->get_int_allocator();
  auto index = node->get_index();
  auto is_allocated = node->get_is_allocated();
  auto objs = _int_allocator->get_objs();
  assert(objs.size() == 1);

  auto dchain = *objs.begin();

  const auto &int_allocator = ingress.get_int_allocator(dchain);
  auto allocated_meta = int_allocator.allocated;

  auto transpiled_integer = transpile(index);

  auto hit_var =
      ingress.allocate_local_auxiliary("is_allocated", 1, {is_allocated});

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append(allocated_meta.get_label());
  ingress.apply_block_builder.append(" = ");
  ingress.apply_block_builder.append(transpiled_integer);
  ingress.apply_block_builder.append(";");
  ingress.apply_block_builder.append_new_line();

  int_allocator.synthesize_query(ingress.apply_block_builder, hit_var);
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::Drop *node) {
  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append("drop();");
  ingress.apply_block_builder.append_new_line();
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::Ignore *node) {
  assert(false && "TODO");
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::SendToController *node) {
  auto cpu_code_path = node->get_cpu_code_path();
  auto dataplane_state = node->get_dataplane_state();

  ingress.apply_block_builder.indent();
  ingress.apply_block_builder.append(INGRESS_SEND_TO_CPU_ACTION);
  ingress.apply_block_builder.append("(");
  ingress.apply_block_builder.append(cpu_code_path);
  ingress.apply_block_builder.append(");");
  ingress.apply_block_builder.append_new_line();

  for (auto dps : dataplane_state) {
    auto hdr_field = ingress.get_cpu_hdr_field(dps);
    assert(hdr_field.valid);

    auto local_var = search_variable(dps);
    assert(local_var.valid);

    if (local_var.var->get_size_bits() == 1) {
      auto actual_size = hdr_field.var->get_size_bits();
      // HACK :<
      // true ? 1 : 0

      ingress.apply_block_builder.indent();
      ingress.apply_block_builder.append("if (");
      ingress.apply_block_builder.append(local_var.var->get_label());
      ingress.apply_block_builder.append(") {");
      ingress.apply_block_builder.append_new_line();
      ingress.apply_block_builder.inc_indentation();

      ingress.apply_block_builder.indent();
      ingress.apply_block_builder.append(hdr_field.var->get_label());
      ingress.apply_block_builder.append(" = ");
      ingress.apply_block_builder.append(actual_size);
      ingress.apply_block_builder.append("w1;");
      ingress.apply_block_builder.append_new_line();

      ingress.apply_block_builder.dec_indentation();

      ingress.apply_block_builder.indent();
      ingress.apply_block_builder.append("} else {");
      ingress.apply_block_builder.append_new_line();
      ingress.apply_block_builder.inc_indentation();

      ingress.apply_block_builder.indent();
      ingress.apply_block_builder.append(hdr_field.var->get_label());
      ingress.apply_block_builder.append(" = ");
      ingress.apply_block_builder.append(actual_size);
      ingress.apply_block_builder.append("w0;");
      ingress.apply_block_builder.append_new_line();

      ingress.apply_block_builder.dec_indentation();

      ingress.apply_block_builder.indent();
      ingress.apply_block_builder.append("}");
      ingress.apply_block_builder.append_new_line();
    } else {
      ingress.apply_block_builder.indent();
      ingress.apply_block_builder.append(hdr_field.var->get_label());
      ingress.apply_block_builder.append(" = ");
      ingress.apply_block_builder.append(local_var.var->get_label());
      ingress.apply_block_builder.append(";");
      ingress.apply_block_builder.append_new_line();
    }
  }
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::CounterRead *node) {
  assert(node);

  auto _counter = node->get_counter();
  auto index = node->get_index();
  auto value = node->get_value();

  auto objs = _counter->get_objs();
  assert(objs.size() == 1);

  auto vector = *objs.begin();

  auto value_label = counter_t::get_value_label(vector);
  auto value_var = ingress.allocate_local_auxiliary(
      value_label, _counter->get_value_size(), {value}, {});

  const auto &counter = ingress.get_counter(vector);
  auto transpiled_integer = transpile(index);

  counter.synthesize_read(ingress.apply_block_builder, transpiled_integer,
                          value_var);
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::CounterIncrement *node) {
  assert(node);

  auto _counter = node->get_counter();
  auto index = node->get_index();
  auto value = node->get_value();

  auto objs = _counter->get_objs();
  assert(objs.size() == 1);

  auto vector = *objs.begin();

  auto value_label = counter_t::get_value_label(vector);
  auto value_var = ingress.allocate_local_auxiliary(
      value_label, _counter->get_value_size(), {value}, {});

  const auto &counter = ingress.get_counter(vector);
  auto transpiled_integer = transpile(index);

  counter.synthesize_inc(ingress.apply_block_builder, transpiled_integer,
                         value_var);
}

void TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                            const target::HashObj *node) {
  auto input = node->get_input();
  auto size = node->get_size();
  auto hash_out = node->get_hash();

  auto inputs = std::vector<std::string>();

  for (auto offset = 0u; offset < input->getWidth(); offset += 8) {
    auto byte = kutil::solver_toolbox.exprBuilder->Extract(input, offset, 8);
    auto byte_transpiled = transpile(byte);
    inputs.push_back(byte_transpiled);
  }

  const auto &hash = ingress.get_or_build_hash(size);

  auto value_label = hash.get_value_label();
  auto value_var =
      ingress.allocate_local_auxiliary(value_label, size, {hash_out}, {});

  hash.synthesize_apply(ingress.apply_block_builder, inputs, value_var);
}

} // namespace tofino
} // namespace synthesizer
}; // namespace synapse
