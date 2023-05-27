#pragma once

namespace synapse {
namespace synthesizer {
namespace tofino {

constexpr char TOFINO_BOILERPLATE_FILE[] = "boilerplate.p4";

constexpr char MARKER_INGRESS_HEADERS_DEF[] = "INGRESS HEADERS DEFINITIONS";
constexpr char MARKER_INGRESS_HEADERS_DECL[] = "INGRESS HEADERS DECLARATIONS";
constexpr char MARKER_INGRESS_METADATA[] = "INGRESS METADATA";
constexpr char MARKER_EGRESS_METADATA[] = "EGRESS METADATA";
constexpr char MARKER_INGRESS_PARSER[] = "INGRESS PARSER";
constexpr char MARKER_INGRESS_STATE[] = "INGRESS STATE";
constexpr char MARKER_INGRESS_APPLY[] = "INGRESS APPLY";
constexpr char MARKER_CPU_HEADER_FIELDS[] = "CPU HEADER FIELDS";

constexpr char INGRESS_FORWARD_ACTION[] = "fwd";
constexpr char INGRESS_DROP_ACTION[] = "drop";
constexpr char INGRESS_FLOOD_ACTION[] = "flood";
constexpr char INGRESS_SEND_TO_CPU_ACTION[] = "send_to_cpu";

constexpr char PARSER_STATE_INIT_LABEL[] = "parse_init";
constexpr char PARSER_ACCEPT[] = "accept";
constexpr char PARSER_REJECT[] = "reject";

constexpr char PARSER_CONDITIONAL_LABEL[] = "conditional_state_t_";
constexpr char PARSER_PACKET_VARIABLE_LABEL[] = "pkt";
constexpr char PARSER_INITIAL_STATE_LABEL[] = "parse_headers";
constexpr char KEY_BYTE_LABEL[] = "key_byte_free";

constexpr char CPU_HEADER_LABEL[] = "cpu";

constexpr char INGRESS_USER_METADATA_VARIABLE[] = "meta";
constexpr char INGRESS_PACKET_HEADER_VARIABLE[] = "hdr";

constexpr char INGRESS_INTRINSIC_META_RESUBMIT_FLAG[] =
    "ig_intr_md.resubmit_flag";
constexpr char INGRESS_INTRINSIC_META_PACKET_VERSION[] =
    "ig_intr_md.packet_version";
constexpr char INGRESS_INTRINSIC_META_INGRESS_PORT[] =
    "ig_intr_md.ingress_port";
constexpr char INGRESS_INTRINSIC_META_TIMESTAMP[] =
    "ig_intr_md.ingress_mac_tstamp";

constexpr int INGRESS_INTRINSIC_META_RESUBMIT_FLAG_SIZE_BITS = 1;
constexpr int INGRESS_INTRINSIC_META_PACKET_VERSION_SIZE_BITS = 2;
constexpr int INGRESS_INTRINSIC_META_INGRESS_PORT_SIZE_BITS = 9;
constexpr int INGRESS_INTRINSIC_META_TIMESTAMP_SIZE_BITS = 48;

constexpr char HDR_ETH[] = "ethernet";
constexpr char HDR_IPV4[] = "ipv4";
constexpr char HDR_IPV4_OPTIONS[] = "ipv4_options";
constexpr char HDR_TCPUDP[] = "tcpudp";

constexpr char HDR_ETH_SRC_ADDR_FIELD[] = "src_addr";
constexpr char HDR_ETH_DST_ADDR_FIELD[] = "dst_addr";
constexpr char HDR_ETH_ETHER_TYPE_FIELD[] = "ether_type";

constexpr char HDR_IPV4_VERSION_FIELD[] = "version";
constexpr char HDR_IPV4_IHL_FIELD[] = "ihl";
constexpr char HDR_IPV4_DSCP_FIELD[] = "dscp";
constexpr char HDR_IPV4_TOT_LEN_FIELD[] = "tot_len";
constexpr char HDR_IPV4_ID_FIELD[] = "id";
constexpr char HDR_IPV4_FRAG_OFF_FIELD[] = "frag_off";
constexpr char HDR_IPV4_TTL_FIELD[] = "ttl";
constexpr char HDR_IPV4_PROTOCOL_FIELD[] = "protocol";
constexpr char HDR_IPV4_CHECK_FIELD[] = "check";
constexpr char HDR_IPV4_SRC_ADDR_FIELD[] = "src_addr";
constexpr char HDR_IPV4_DST_ADDR_FIELD[] = "dst_addr";

constexpr char HDR_IPV4_OPTIONS_VALUE_FIELD[] = "value";

constexpr char HDR_TCPUDP_SRC_PORT_FIELD[] = "src_port";
constexpr char HDR_TCPUDP_DST_PORT_FIELD[] = "dst_port";

} // namespace tofino
} // namespace synthesizer
} // namespace synapse