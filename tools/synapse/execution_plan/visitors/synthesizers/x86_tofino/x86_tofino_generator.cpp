#include "x86_tofino_generator.h"
#include "klee-util.h"

#include "../../../../log.h"
#include "../../../modules/modules.h"
#include "../util.h"
#include "transpiler.h"

#include <sstream>

#define ADD_NODE_COMMENT(module)                                               \
  {                                                                            \
    if (module->get_node()) {                                                  \
      nf_process_builder.indent();                                             \
      nf_process_builder.append("// node ");                                   \
      nf_process_builder.append((module)->get_node()->get_id());               \
      nf_process_builder.append_new_line();                                    \
    }                                                                          \
  }

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

std::string x86TofinoGenerator::transpile(klee::ref<klee::Expr> expr) {
  return transpiler.transpile(expr);
}

variable_query_t x86TofinoGenerator::search_variable(std::string symbol) const {
  if (symbol == BDD::symbex::PORT || symbol == BDD::symbex::PORT2) {
    auto in_port_var = headers.query_hdr_field(CPU, CPU_IN_PORT);

    if (in_port_var.valid) {
      return in_port_var;
    }
  } else if (symbol == BDD::symbex::CPU_CODE_PATH) {
    auto code_path_var = headers.query_hdr_field(CPU, CPU_CODE_PATH);

    if (code_path_var.valid) {
      return code_path_var;
    }
  }

  auto local_var = vars.get(symbol);

  if (local_var.valid) {
    return local_var;
  }

  return variable_query_t();
}

variable_query_t
x86TofinoGenerator::search_variable(klee::ref<klee::Expr> expr) const {
  auto local_var = vars.get(expr);

  if (local_var.valid) {
    return local_var;
  }

  auto hdr_field = headers.query_hdr_field_from_chunk(expr);

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

bool get_dataplane_table_names(const ExecutionPlan &ep, addr_t obj,
                               std::unordered_set<std::string> &table_names) {
  auto mb = ep.get_memory_bank();

  if (!mb->check_placement_decision(obj, PlacementDecision::TofinoTable)) {
    return false;
  }

  auto tmb = ep.get_memory_bank<targets::tofino::TofinoMemoryBank>(Tofino);
  auto implementations = tmb->get_implementations(obj);

  for (auto implementation : implementations) {
    auto type = implementation->get_type();

    assert(type == targets::tofino::DataStructure::TABLE);

    auto table =
        static_cast<const targets::tofino::Table *>(implementation.get());
    auto table_name = table->get_name();
    table_names.insert(table_name);
  }

  return true;
}

void x86TofinoGenerator::init_state(ExecutionPlan ep) {
  auto mb = ep.get_memory_bank<target::x86TofinoMemoryBank>(x86_Tofino);
  auto data_structures = mb->get_data_structures();

  for (auto ds : data_structures) {
    switch (ds->type) {
    case target::x86TofinoMemoryBank::ds_type_t::MAP: {
      auto map_ds = static_cast<target::x86TofinoMemoryBank::map_t *>(ds.get());

      auto label = "map_" + std::to_string(map_ds->node_id);

      auto map_var = Variable(label, 64);
      map_var.set_addr(map_ds->addr);

      auto value_type = transpiler.size_to_type(map_ds->value_size);

      state_decl_builder.indent();
      state_decl_builder.append("std::unique_ptr<Map<");
      state_decl_builder.append(value_type);
      state_decl_builder.append(">> ");
      state_decl_builder.append(label);
      state_decl_builder.append(";");
      state_decl_builder.append_new_line();

      state_init_builder.indent();
      state_init_builder.append(label);
      state_init_builder.append(" = Map<");
      state_init_builder.append(value_type);
      state_init_builder.append(">::build(");

      std::unordered_set<std::string> table_names;

      auto has_dataplane =
          get_dataplane_table_names(ep, map_ds->addr, table_names);

      if (has_dataplane) {
        state_init_builder.append("{");
        for (const auto &table_name : table_names) {
          state_init_builder.append("\"");
          state_init_builder.append(table_name);
          state_init_builder.append("\"");
          state_init_builder.append(",");
        }
        state_init_builder.append("}");
      }

      state_init_builder.append(");");
      state_init_builder.append_new_line();

      vars.append(map_var);
    } break;

    case target::x86TofinoMemoryBank::ds_type_t::DCHAIN: {
      auto dchain_ds =
          static_cast<target::x86TofinoMemoryBank::dchain_t *>(ds.get());

      auto label = "dchain_" + std::to_string(dchain_ds->node_id);
      auto dchain_var = Variable(label, 64);
      dchain_var.set_addr(dchain_ds->addr);

      state_decl_builder.indent();
      state_decl_builder.append("std::unique_ptr<Dchain> ");
      state_decl_builder.append(label);
      state_decl_builder.append(";");
      state_decl_builder.append_new_line();

      state_init_builder.indent();
      state_init_builder.append(label);
      state_init_builder.append(" = Dchain::build(");
      state_init_builder.append(dchain_ds->index_range);
      state_init_builder.append(");");
      state_init_builder.append_new_line();

      vars.append(dchain_var);
    } break;
    }
  }

  // For multiple SendToController operations, we have multiple next_time
  // symbols
  for (auto time : mb->get_time()) {
    assert(!time.expr.isNull());
    auto time_var =
        Variable(TIME_VAR_LABEL, time.expr->getWidth(), {time.label});
    time_var.add_expr(time.expr);
    vars.append(time_var);
  }

  auto packet_len_var =
      Variable(PACKET_LENGTH_VAR_LABEL, 32, {BDD::symbex::PACKET_LENGTH});
  vars.append(packet_len_var);

  // HACK: we don't care about this symbol
  auto number_of_freed_flows = Variable(BDD::symbex::EXPIRE_MAP_FREED_FLOWS, 32,
                                        {BDD::symbex::EXPIRE_MAP_FREED_FLOWS});
  vars.append(number_of_freed_flows);

  nf_process_builder.indent();
  nf_process_builder.append(number_of_freed_flows.get_type());
  nf_process_builder.append(" ");
  nf_process_builder.append(number_of_freed_flows.get_label());
  nf_process_builder.append(" = 0");
  nf_process_builder.append(";");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(ExecutionPlan ep) {
  init_state(ep);

  ExecutionPlanVisitor::visit(ep);

  std::stringstream state_decl_code;
  std::stringstream state_init_code;
  std::stringstream nf_process_code;
  std::stringstream cpu_hdr_fields_code;

  state_decl_builder.dump(state_decl_code);
  state_init_builder.dump(state_init_code);
  nf_process_builder.dump(nf_process_code);
  cpu_hdr_fields_builder.dump(cpu_hdr_fields_code);

  fill_mark(MARKER_STATE_DECL, state_decl_code.str());
  fill_mark(MARKER_STATE_INIT, state_init_code.str());
  fill_mark(MARKER_NF_PROCESS, nf_process_code.str());
  fill_mark(MARKER_CPU_HEADER, cpu_hdr_fields_code.str());
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node) {
  auto mod = ep_node->get_module();
  auto next = ep_node->get_next();

  log(ep_node);

  ADD_NODE_COMMENT(ep_node->get_module());
  mod->visit(*this, ep_node);

  if (ep_node->is_terminal_node()) {
    auto closed = pending_ifs.close();
    for (auto i = 0; i < closed; i++) {
      vars.pop();
    }
  }

  for (auto branch : next) {
    branch->visit(*this);
  }
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketParseCPU *node) {
  auto dataplane_state = node->get_dataplane_state();
  auto fields = std::vector<hdr_field_t>();

  fields.emplace_back(CPU_CODE_PATH, HDR_CPU_CODE_PATH_FIELD, 16);
  fields.emplace_back(CPU_IN_PORT, HDR_CPU_IN_PORT_FIELD, 16);
  fields.emplace_back(CPU_OUT_PORT, HDR_CPU_OUT_PORT_FIELD, 16);

  for (auto state : dataplane_state) {
    auto label = state.label;
    auto expr = state.expr;
    auto size = expr->getWidth();

    auto field = hdr_field_t(CPU_DATA_FIELD, label, size);
    auto var =
        Variable(std::string(HDR_CPU_VARIABLE) + "->" + label, size, {label});
    auto hs = kutil::get_symbol(expr);
    if (hs.first && hs.second == label) {
      var.add_expr(expr);
    }

    fields.push_back(field);
    vars.append(var);

    cpu_hdr_fields_builder.indent();

    if (is_primitive_type(field.get_size_bits())) {
      cpu_hdr_fields_builder.append(field.get_type());
    } else {
      cpu_hdr_fields_builder.append("uint8_t");
    }

    cpu_hdr_fields_builder.append(" ");
    cpu_hdr_fields_builder.append(field.get_label());

    if (!is_primitive_type(field.get_size_bits())) {
      assert(field.get_size_bits() % 8 == 0);
      cpu_hdr_fields_builder.append("[");
      cpu_hdr_fields_builder.append(field.get_size_bits() / 8);
      cpu_hdr_fields_builder.append("]");
    }

    cpu_hdr_fields_builder.append(";");
    cpu_hdr_fields_builder.append_new_line();
  }

  auto header = Header(CPU, HDR_CPU_VARIABLE, fields);
  headers.add(header);

  nf_process_builder.indent();
  nf_process_builder.append("auto ");
  nf_process_builder.append(HDR_CPU_VARIABLE);
  nf_process_builder.append(" = ");
  nf_process_builder.append(PACKET_VAR_LABEL);
  nf_process_builder.append(".parse_cpu();");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::Drop *node) {
  assert(node);

  nf_process_builder.indent();
  nf_process_builder.append("return false;");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::ForwardThroughTofino *node) {
  assert(node);
  auto port = node->get_port();

  auto out_port_var = headers.query_hdr_field(CPU, CPU_OUT_PORT);
  assert(out_port_var.valid);

  nf_process_builder.indent();
  nf_process_builder.append(out_port_var.var->get_label());
  nf_process_builder.append(" = ");
  nf_process_builder.append(port);
  nf_process_builder.append(";");
  nf_process_builder.append_new_line();

  nf_process_builder.indent();
  nf_process_builder.append("return true;");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketParseEthernet *node) {
  assert(node);

  const hdr_field_t eth_dst_addr{ETH_DST_ADDR, HDR_ETH_DST_ADDR_FIELD, 48};
  const hdr_field_t eth_src_addr{ETH_SRC_ADDR, HDR_ETH_SRC_ADDR_FIELD, 48};
  const hdr_field_t eth_ether_type{ETH_ETHER_TYPE, HDR_ETH_ETHER_TYPE_FIELD,
                                   16};

  std::vector<hdr_field_t> fields = {
      eth_dst_addr,
      eth_src_addr,
      eth_ether_type,
  };

  auto chunk = node->get_chunk();
  auto header = Header(ETHERNET, HDR_ETH_VARIABLE, chunk, fields);

  headers.add(header);

  nf_process_builder.indent();
  nf_process_builder.append("auto ");
  nf_process_builder.append(HDR_ETH_VARIABLE);
  nf_process_builder.append(" = ");
  nf_process_builder.append(PACKET_VAR_LABEL);
  nf_process_builder.append(".parse_ethernet();");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketModifyEthernet *node) {
  assert(node);

  auto ethernet_hdr = headers.get_hdr(ETHERNET);
  auto modifications = node->get_modifications();

  assert(ethernet_hdr.valid);
  assert(ethernet_hdr.offset_bits == 0);

  for (auto mod : modifications) {
    auto byte = mod.byte;
    auto expr = mod.expr;

    auto transpiled_byte =
        transpiler.transpile_lvalue_byte(*ethernet_hdr.var, byte * 8);
    auto transpiled_expr = transpile(expr);

    nf_process_builder.indent();
    nf_process_builder.append(transpiled_byte);
    nf_process_builder.append(" = ");
    nf_process_builder.append(transpiled_expr);
    nf_process_builder.append(";");
    nf_process_builder.append_new_line();
  }
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketParseIPv4 *node) {
  assert(node);

  const hdr_field_t ver_ihl{IPV4_VERSION_IHL, HDR_IPV4_VERSION_IHL_FIELD, 8};
  const hdr_field_t ecn_dscp{IPV4_ECN_DSCP, HDR_IPV4_ECN_DSCP_FIELD, 8};
  const hdr_field_t tot_len{IPV4_TOT_LEN, HDR_IPV4_TOT_LEN_FIELD, 16};
  const hdr_field_t id{IPV4_ID, HDR_IPV4_ID_FIELD, 16};
  const hdr_field_t frag_off{IPV4_FRAG_OFF, HDR_IPV4_FRAG_OFF_FIELD, 16};
  const hdr_field_t ttl{IPV4_TTL, HDR_IPV4_TTL_FIELD, 8};
  const hdr_field_t protocol{IPV4_PROTOCOL, HDR_IPV4_PROTOCOL_FIELD, 8};
  const hdr_field_t check{IPV4_CHECK, HDR_IPV4_CHECK_FIELD, 16};
  const hdr_field_t src_ip{IPV4_SRC_IP, HDR_IPV4_SRC_IP_FIELD, 32};
  const hdr_field_t dst_ip{IPV4_DST_IP, HDR_IPV4_DST_IP_FIELD, 32};

  std::vector<hdr_field_t> fields = {
      ver_ihl, ecn_dscp, tot_len, id,     frag_off,
      ttl,     protocol, check,   src_ip, dst_ip,
  };

  auto chunk = node->get_chunk();
  auto header = Header(IPV4, HDR_IPV4_VARIABLE, chunk, fields);

  headers.add(header);

  nf_process_builder.indent();
  nf_process_builder.append("auto ");
  nf_process_builder.append(HDR_IPV4_VARIABLE);
  nf_process_builder.append(" = ");
  nf_process_builder.append(PACKET_VAR_LABEL);
  nf_process_builder.append(".parse_ipv4();");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketModifyIPv4 *node) {
  assert(node);

  auto ipv4_hdr = headers.get_hdr(IPV4);
  auto modifications = node->get_modifications();

  assert(ipv4_hdr.valid);
  assert(ipv4_hdr.offset_bits == 0);

  for (auto mod : modifications) {
    auto byte = mod.byte;
    auto expr = mod.expr;

    auto transpiled_byte =
        transpiler.transpile_lvalue_byte(*ipv4_hdr.var, byte * 8);
    auto transpiled_expr = transpile(expr);

    nf_process_builder.indent();
    nf_process_builder.append(transpiled_byte);
    nf_process_builder.append(" = ");
    nf_process_builder.append(transpiled_expr);
    nf_process_builder.append(";");
    nf_process_builder.append_new_line();
  }
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketParseIPv4Options *node) {
  assert(node);

  auto chunk = node->get_chunk();
  auto length = node->get_length();

  const hdr_field_t value{IPV4_OPTIONS_VALUE, HDR_IPV4_OPTIONS_VALUE_FIELD,
                          length};
  std::vector<hdr_field_t> fields = {value};
  auto header = Header(IPV4_OPTIONS, HDR_IPV4_OPTIONS_VARIABLE, chunk, fields);

  headers.add(header);

  nf_process_builder.indent();
  nf_process_builder.append("auto ");
  nf_process_builder.append(HDR_IPV4_OPTIONS_VARIABLE);
  nf_process_builder.append(" = ");
  nf_process_builder.append(PACKET_VAR_LABEL);
  nf_process_builder.append(".parse_tcpudp();");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketModifyIPv4Options *node) {
  assert(false && "TODO");
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketParseTCPUDP *node) {
  assert(node);

  const hdr_field_t src_port{TCPUDP_SRC_PORT, HDR_TCPUDP_SRC_PORT_FIELD, 16};
  const hdr_field_t dst_port{TCPUDP_DST_PORT, HDR_TCPUDP_DST_PORT_FIELD, 16};

  std::vector<hdr_field_t> fields = {
      src_port,
      dst_port,
  };

  auto chunk = node->get_chunk();
  auto header = Header(TCPUDP, HDR_TCPUDP_VARIABLE, chunk, fields);

  headers.add(header);

  nf_process_builder.indent();
  nf_process_builder.append("auto ");
  nf_process_builder.append(HDR_TCPUDP_VARIABLE);
  nf_process_builder.append(" = ");
  nf_process_builder.append(PACKET_VAR_LABEL);
  nf_process_builder.append(".parse_tcpudp();");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketModifyTCPUDP *node) {
  assert(node);

  auto tcpudp_hdr = headers.get_hdr(TCPUDP);
  auto modifications = node->get_modifications();

  assert(tcpudp_hdr.valid);
  assert(tcpudp_hdr.offset_bits == 0);

  for (auto mod : modifications) {
    auto byte = mod.byte;
    auto expr = mod.expr;

    auto transpiled_byte =
        transpiler.transpile_lvalue_byte(*tcpudp_hdr.var, byte * 8);
    auto transpiled_expr = transpile(expr);

    nf_process_builder.indent();
    nf_process_builder.append(transpiled_byte);
    nf_process_builder.append(" = ");
    nf_process_builder.append(transpiled_expr);
    nf_process_builder.append(";");
    nf_process_builder.append_new_line();
  }
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::PacketModifyChecksums *node) {
  auto ip_header_addr = node->get_ip_header_addr();
  auto l4_header_addr = node->get_l4_header_addr();
  auto p_addr = node->get_p_addr();
  auto generated_symbols = node->get_generated_symbols();

  assert(!ip_header_addr.isNull());
  assert(!l4_header_addr.isNull());
  assert(!p_addr.isNull());

  auto ip_hdr_var = headers.get_hdr(IPV4);
  auto l4_hdr_var = headers.get_hdr(TCPUDP);

  assert(ip_hdr_var.valid);
  assert(l4_hdr_var.valid);

  assert(generated_symbols.size() == 1);
  auto checksum_symbol = get_label(generated_symbols, BDD::symbex::CHECKSUM);
  auto checksum_label = "*" + checksum_symbol; // will be a pointer

  assert(ip_hdr_var.var->has_expr());
  auto ip_hdr_label = ip_hdr_var.var->get_label();
  auto checksum_ip_offset_bytes = 10;

  auto checksum_var = Variable(checksum_label, 16, {checksum_symbol});
  auto checksum_type = checksum_var.get_type();
  vars.append(checksum_var);

  nf_process_builder.indent();
  nf_process_builder.append(checksum_type);
  nf_process_builder.append(" ");
  nf_process_builder.append(checksum_label);
  nf_process_builder.append(" = ");
  nf_process_builder.append("(");
  nf_process_builder.append(checksum_type);
  nf_process_builder.append("*)(");
  nf_process_builder.append("((uint8_t*)");
  nf_process_builder.append(ip_hdr_var.var->get_label());
  nf_process_builder.append(")+");
  nf_process_builder.append(checksum_ip_offset_bytes);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();

  nf_process_builder.indent();
  nf_process_builder.append("update_ipv4_tcpudp_checksums(");
  nf_process_builder.append(ip_hdr_var.var->get_label());
  nf_process_builder.append(", ");
  nf_process_builder.append(l4_hdr_var.var->get_label());
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::If *node) {
  assert(node);

  auto condition = node->get_condition();
  auto condition_transpiled = transpile(condition);

  nf_process_builder.indent();
  nf_process_builder.append("if (");
  nf_process_builder.append(condition_transpiled);
  nf_process_builder.append(") {");
  nf_process_builder.append_new_line();

  nf_process_builder.inc_indentation();

  vars.push();
  pending_ifs.push();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::Then *node) {
  assert(node);
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::Else *node) {
  assert(node);

  vars.push();

  nf_process_builder.indent();
  nf_process_builder.append("else {");
  nf_process_builder.append_new_line();
  nf_process_builder.inc_indentation();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::MapGet *node) {
  assert(node);

  auto map_addr = node->get_map_addr();
  auto key = node->get_key();
  auto map_has_this_key = node->get_map_has_this_key();
  auto value_out = node->get_value_out();

  auto generated_symbols = node->get_generated_symbols();

  assert(!key.isNull());
  assert(!value_out.isNull());

  if (!map_has_this_key.isNull()) {
    auto map_has_this_key_label =
        get_label(generated_symbols, BDD::symbex::MAP_HAS_THIS_KEY);
    auto contains_var =
        Variable(map_has_this_key_label, map_has_this_key->getWidth(),
                 {map_has_this_key_label});
    vars.append(contains_var);
  }

  auto allocated_index_label = std::string();

  if (has_label(generated_symbols, BDD::symbex::MAP_ALLOCATED_INDEX)) {
    allocated_index_label =
        get_label(generated_symbols, BDD::symbex::MAP_ALLOCATED_INDEX);
  } else if (has_label(generated_symbols, BDD::symbex::VECTOR_VALUE_SYMBOL)) {
    allocated_index_label =
        get_label(generated_symbols, BDD::symbex::VECTOR_VALUE_SYMBOL);
  } else {
    assert(false && "No valid generated symbol");
  }

  auto value_var = Variable(allocated_index_label, value_out->getWidth(),
                            {allocated_index_label});
  value_var.add_expr(value_out);
  vars.append(value_var);

  nf_process_builder.indent();
  nf_process_builder.append(value_var.get_type());
  nf_process_builder.append(" ");
  nf_process_builder.append(value_var.get_label());
  nf_process_builder.append(";");
  nf_process_builder.append_new_line();

  auto map = vars.get(map_addr);
  assert(map.valid);

  auto key_label_base = map.var->get_label() + "_key";
  auto key_label = vars.get_new_label(key_label_base);
  auto key_var = Variable(key_label, key->getWidth());

  vars.append(key_var);

  assert(key->getWidth() > 0);
  assert(key->getWidth() % 8 == 0);

  nf_process_builder.indent();
  nf_process_builder.append("bytes_t ");
  nf_process_builder.append(key_label);
  nf_process_builder.append("(");
  nf_process_builder.append(key->getWidth() / 8);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();

  for (bits_t b = 0u; b < key->getWidth(); b += 8) {
    auto byte = kutil::solver_toolbox.exprBuilder->Extract(key, b, 8);
    auto byte_transpiled = transpile(byte);

    nf_process_builder.indent();
    nf_process_builder.append(key_label);
    nf_process_builder.append("[");
    nf_process_builder.append(b / 8);
    nf_process_builder.append("]");
    nf_process_builder.append(" = ");
    nf_process_builder.append(byte_transpiled);
    nf_process_builder.append(";");
    nf_process_builder.append_new_line();
  }

  nf_process_builder.indent();

  if (!map_has_this_key.isNull()) {
    auto map_has_this_key_label =
        get_label(generated_symbols, BDD::symbex::MAP_HAS_THIS_KEY);
    auto contains_var = vars.get(map_has_this_key_label);
    assert(contains_var.valid);
    assert(contains_var.offset_bits == 0);
    nf_process_builder.append("auto ");
    nf_process_builder.append(contains_var.var->get_label());
    nf_process_builder.append(" = ");
  }

  nf_process_builder.append("state->");
  nf_process_builder.append(map.var->get_label());
  nf_process_builder.append("->get(");
  nf_process_builder.append(key_label);
  nf_process_builder.append(", ");
  nf_process_builder.append(value_var.get_label());
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::MapPut *node) {
  assert(node);

  auto map_addr = node->get_map_addr();
  auto key = node->get_key();
  auto value = node->get_value();

  assert(!key.isNull());
  assert(!value.isNull());

  auto map = vars.get(map_addr);
  assert(map.valid);

  auto key_label_base = map.var->get_label() + "_key";
  auto key_label = vars.get_new_label(key_label_base);
  auto key_var = Variable(key_label, key->getWidth());
  vars.append(key_var);

  auto value_label_base = map.var->get_label() + "_value";
  auto value_label = vars.get_new_label(value_label_base);
  auto value_var = Variable(value_label, value->getWidth());
  vars.append(value_var);

  assert(key->getWidth() > 0);
  assert(key->getWidth() % 8 == 0);

  nf_process_builder.indent();
  nf_process_builder.append("bytes_t ");
  nf_process_builder.append(key_label);
  nf_process_builder.append("(");
  nf_process_builder.append(key->getWidth() / 8);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();

  for (bits_t b = 0u; b < key->getWidth(); b += 8) {
    auto byte = kutil::solver_toolbox.exprBuilder->Extract(key, b, 8);
    auto byte_transpiled = transpile(byte);

    nf_process_builder.indent();
    nf_process_builder.append(key_label);
    nf_process_builder.append("[");
    nf_process_builder.append(b / 8);
    nf_process_builder.append("]");
    nf_process_builder.append(" = ");
    nf_process_builder.append(byte_transpiled);
    nf_process_builder.append(";");
    nf_process_builder.append_new_line();
  }

  if (is_primitive_type(value->getWidth())) {
    auto value_transpiled = transpile(value);

    nf_process_builder.indent();
    nf_process_builder.append(value_var.get_type());
    nf_process_builder.append(" ");
    nf_process_builder.append(value_var.get_label());
    nf_process_builder.append(" = ");
    nf_process_builder.append(value_transpiled);
    nf_process_builder.append(";");
    nf_process_builder.append_new_line();
  } else {
    nf_process_builder.indent();
    nf_process_builder.append("bytes_t ");
    nf_process_builder.append(value_var.get_label());
    nf_process_builder.append("(");
    nf_process_builder.append(value->getWidth() / 8);
    nf_process_builder.append(");");
    nf_process_builder.append_new_line();

    for (bits_t b = 0u; b < value->getWidth(); b += 8) {
      auto byte = kutil::solver_toolbox.exprBuilder->Extract(value, b, 8);
      auto byte_transpiled = transpile(byte);

      nf_process_builder.indent();
      nf_process_builder.append(value_var.get_label());
      nf_process_builder.append("[");
      nf_process_builder.append(b / 8);
      nf_process_builder.append("]");
      nf_process_builder.append(" = ");
      nf_process_builder.append(byte_transpiled);
      nf_process_builder.append(";");
      nf_process_builder.append_new_line();
    }
  }

  nf_process_builder.indent();
  nf_process_builder.append("state->");
  nf_process_builder.append(map.var->get_label());
  nf_process_builder.append("->put(");
  nf_process_builder.append(key_label);
  nf_process_builder.append(", ");
  nf_process_builder.append(value_var.get_label());
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::EtherAddrHash *node) {
  assert(node);

  auto addr = node->get_addr();
  auto generated_symbol = node->get_generated_symbol();

  auto addr_transpiled = transpile(addr);
  auto hash_var = Variable(generated_symbol.label, generated_symbol.expr);

  vars.append(hash_var);

  nf_process_builder.indent();
  nf_process_builder.append("auto ");
  nf_process_builder.append(hash_var.get_label());
  nf_process_builder.append(" = ");
  nf_process_builder.append("ether_addr_hash(");
  nf_process_builder.append(addr_transpiled);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::DchainAllocateNewIndex *node) {
  assert(node);

  auto dchain_addr = node->get_dchain_addr();
  auto time = node->get_time();
  auto index_out = node->get_index_out();
  auto success = node->get_success();
  auto generated_symbols = node->get_generated_symbols();

  assert(!time.isNull());
  assert(!index_out.isNull());
  assert(!success.isNull());
  assert(generated_symbols.size() == 2);

  auto out_of_space =
      get_symbol(generated_symbols, BDD::symbex::DCHAIN_OUT_OF_SPACE);
  auto out_of_space_var =
      Variable(out_of_space.label, success->getWidth(), {out_of_space.label});
  auto out_of_space_type = out_of_space_var.get_type();
  out_of_space_var.add_expr(out_of_space.expr);
  vars.append(out_of_space_var);

  auto new_index = get_symbol(generated_symbols, BDD::symbex::DCHAIN_NEW_INDEX);
  auto new_index_var =
      Variable(new_index.label, success->getWidth(), {new_index.label});
  auto new_index_type = new_index_var.get_type();
  new_index_var.add_expr(new_index.expr);
  vars.append(new_index_var);

  auto dchain = vars.get(dchain_addr);
  auto time_transpiled = transpile(time);

  assert(dchain.valid);

  nf_process_builder.indent();
  nf_process_builder.append(new_index_type);
  nf_process_builder.append(" ");
  nf_process_builder.append(new_index_var.get_label());
  nf_process_builder.append(";");
  nf_process_builder.append_new_line();

  nf_process_builder.indent();
  nf_process_builder.append(out_of_space_type);
  nf_process_builder.append(" ");
  nf_process_builder.append(out_of_space_var.get_label());
  nf_process_builder.append(" = ");
  nf_process_builder.append("state->");
  nf_process_builder.append(dchain.var->get_label());
  nf_process_builder.append("->allocate_new_index(");
  nf_process_builder.append(new_index_var.get_label());
  nf_process_builder.append(", ");
  nf_process_builder.append(time_transpiled);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::DchainIsIndexAllocated *node) {
  assert(node);

  auto dchain_addr = node->get_dchain_addr();
  auto index = node->get_index();
  auto is_index_allocated = node->get_is_allocated();

  auto generated_symbols = node->get_generated_symbols();

  assert(!index.isNull());
  assert(!is_index_allocated.isNull());

  assert(generated_symbols.size() == 1);
  auto is_allocated_label =
      get_label(generated_symbols, BDD::symbex::DCHAIN_IS_INDEX_ALLOCATED);
  auto is_allocated_var = Variable(
      is_allocated_label, is_index_allocated->getWidth(), {is_allocated_label});
  auto is_allocated_type = is_allocated_var.get_type();
  is_allocated_var.add_expr(is_index_allocated);
  vars.append(is_allocated_var);

  auto dchain = vars.get(dchain_addr);
  auto index_transpiled = transpile(index);

  assert(dchain.valid);

  nf_process_builder.indent();
  nf_process_builder.append(is_allocated_type);
  nf_process_builder.append(" ");
  nf_process_builder.append(is_allocated_var.get_label());
  nf_process_builder.append(" = ");
  nf_process_builder.append("state->");
  nf_process_builder.append(dchain.var->get_label());
  nf_process_builder.append("->is_index_allocated(");
  nf_process_builder.append(index_transpiled);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::DchainRejuvenateIndex *node) {
  assert(node);

  auto dchain_addr = node->get_dchain_addr();
  auto time = node->get_time();
  auto index = node->get_index();

  assert(!time.isNull());
  assert(!index.isNull());

  auto dchain = vars.get(dchain_addr);
  auto index_transpiled = transpile(index);
  auto time_transpiled = transpile(time);

  assert(dchain.valid);

  nf_process_builder.indent();
  nf_process_builder.append("state->");
  nf_process_builder.append(dchain.var->get_label());
  nf_process_builder.append("->rejuvenate_index(");
  nf_process_builder.append(index_transpiled);
  nf_process_builder.append(", ");
  nf_process_builder.append(time_transpiled);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

void x86TofinoGenerator::visit(const ExecutionPlanNode *ep_node,
                               const target::HashObj *node) {
  assert(node);

  auto input = node->get_input();
  auto hash = node->get_hash();
  auto size = node->get_size();

  assert(!input.isNull());
  assert(!hash.isNull());

  auto obj_label_base = "obj";
  auto obj_label = vars.get_new_label(obj_label_base);
  auto obj_var = Variable(obj_label, size);
  vars.append(obj_var);

  assert(size > 0);
  assert(size % 8 == 0);

  nf_process_builder.indent();
  nf_process_builder.append("bytes_t ");
  nf_process_builder.append(obj_label);
  nf_process_builder.append("(");
  nf_process_builder.append(size / 8);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();

  for (bits_t b = 0u; b < input->getWidth(); b += 8) {
    auto byte = kutil::solver_toolbox.exprBuilder->Extract(input, b, 8);
    auto byte_transpiled = transpile(byte);

    nf_process_builder.indent();
    nf_process_builder.append(obj_label);
    nf_process_builder.append("[");
    nf_process_builder.append(b / 8);
    nf_process_builder.append("]");
    nf_process_builder.append(" = ");
    nf_process_builder.append(byte_transpiled);
    nf_process_builder.append(";");
    nf_process_builder.append_new_line();
  }

  auto hash_label_base = "hash";
  auto hash_label = vars.get_new_label(hash_label_base);
  auto hash_var = Variable(hash_label, size);
  hash_var.add_expr(hash);
  vars.append(hash_var);

  nf_process_builder.indent();
  nf_process_builder.append(hash_var.get_type());
  nf_process_builder.append(" ");
  nf_process_builder.append(hash_var.get_label());
  nf_process_builder.append(" = ");
  nf_process_builder.append("hash_obj(");
  nf_process_builder.append(obj_label);
  nf_process_builder.append(");");
  nf_process_builder.append_new_line();
}

} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse