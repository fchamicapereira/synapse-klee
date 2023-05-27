#pragma once

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

constexpr char BOILERPLATE_FILE[] = "boilerplate.cpp";

constexpr char MARKER_STATE_DECL[] = "NF STATE DECL";
constexpr char MARKER_STATE_INIT[] = "NF STATE INIT";
constexpr char MARKER_NF_PROCESS[] = "NF PROCESS";
constexpr char MARKER_CPU_HEADER[] = "CPU HEADER DATAPLANE STATE FIELDS";

constexpr char DROP_PORT_VALUE[] = "DROP";

constexpr char PACKET_VAR_LABEL[] = "pkt";
constexpr char PACKET_LENGTH_VAR_LABEL[] = "pkt.size";
constexpr char CPU_VAR_LABEL[] = "cpu";
constexpr char TIME_VAR_LABEL[] = "now";

constexpr char HDR_CPU_VARIABLE[] = "cpu";
constexpr char HDR_ETH_VARIABLE[] = "ether";
constexpr char HDR_IPV4_VARIABLE[] = "ip";
constexpr char HDR_IPV4_OPTIONS_VARIABLE[] = "ip_options";
constexpr char HDR_TCPUDP_VARIABLE[] = "tcpudp";

constexpr char HDR_CPU_CODE_PATH_FIELD[] = "code_path";
constexpr char HDR_CPU_IN_PORT_FIELD[] = "in_port";
constexpr char HDR_CPU_OUT_PORT_FIELD[] = "out_port";

constexpr char HDR_ETH_DST_ADDR_FIELD[] = "dst_mac";
constexpr char HDR_ETH_SRC_ADDR_FIELD[] = "src_mac";
constexpr char HDR_ETH_ETHER_TYPE_FIELD[] = "ether_type";

constexpr char HDR_IPV4_VERSION_IHL_FIELD[] = "version_ihl";
constexpr char HDR_IPV4_ECN_DSCP_FIELD[] = "ecn_dscp";
constexpr char HDR_IPV4_TOT_LEN_FIELD[] = "tot_len";
constexpr char HDR_IPV4_ID_FIELD[] = "id";
constexpr char HDR_IPV4_FRAG_OFF_FIELD[] = "frag_off";
constexpr char HDR_IPV4_TTL_FIELD[] = "ttl";
constexpr char HDR_IPV4_PROTOCOL_FIELD[] = "protocol";
constexpr char HDR_IPV4_CHECK_FIELD[] = "check";
constexpr char HDR_IPV4_SRC_IP_FIELD[] = "src_ip";
constexpr char HDR_IPV4_DST_IP_FIELD[] = "dst_ip";

constexpr char HDR_IPV4_OPTIONS_VALUE_FIELD[] = "value";

constexpr char HDR_TCPUDP_SRC_PORT_FIELD[] = "src_port";
constexpr char HDR_TCPUDP_DST_PORT_FIELD[] = "dst_port";

} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse