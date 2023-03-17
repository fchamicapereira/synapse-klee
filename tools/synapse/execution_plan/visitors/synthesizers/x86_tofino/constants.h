#pragma once

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

constexpr char BOILERPLATE_FILE[] = "boilerplate.cpp";

constexpr char MARKER_STATE_DECL[] = "NF STATE DECL";
constexpr char MARKER_STATE_INIT[] = "NF STATE INIT";
constexpr char MARKER_NF_PROCESS[] = "NF_PROCESS";

constexpr char DROP_PORT_VALUE[] = "DROP";

constexpr char PACKET_VAR_LABEL[] = "pkt";
constexpr char CPU_VAR_LABEL[] = "cpu";
constexpr char TIME_VAR_LABEL[] = "now";

constexpr char HDR_CPU_VARIABLE[] = "cpu";
constexpr char HDR_ETH_VARIABLE[] = "ether";
constexpr char HDR_IPV4_VARIABLE[] = "ip";
constexpr char HDR_TCPUDP_VARIABLE[] = "tcpudp";

constexpr char HDR_CPU_CODE_PATH_FIELD[] = "code_path";
constexpr char HDR_CPU_IN_PORT_FIELD[] = "in_port";
constexpr char HDR_CPU_OUT_PORT_FIELD[] = "out_port";

constexpr char HDR_ETH_SRC_ADDR_FIELD[] = "src_addr";
constexpr char HDR_ETH_DST_ADDR_FIELD[] = "dst_addr";
constexpr char HDR_ETH_ETHER_TYPE_FIELD[] = "ether_type";

constexpr char HDR_IPV4_VERSION_FIELD[] = "version";
constexpr char HDR_IPV4_IHL_FIELD[] = "ihl";
constexpr char HDR_IPV4_DSCP_FIELD[] = "dscp";
constexpr char HDR_IPV4_TOTAL_LEN_FIELD[] = "total_len";
constexpr char HDR_IPV4_ID_FIELD[] = "identification";
constexpr char HDR_IPV4_FLAGS_FIELD[] = "flags";
constexpr char HDR_IPV4_FRAG_OFF_FIELD[] = "frag_offset";
constexpr char HDR_IPV4_TTL_FIELD[] = "ttl";
constexpr char HDR_IPV4_PROTO_FIELD[] = "protocol";
constexpr char HDR_IPV4_CHKSUM_FIELD[] = "hdr_checksum";
constexpr char HDR_IPV4_SRC_ADDR_FIELD[] = "src_addr";
constexpr char HDR_IPV4_DST_ADDR_FIELD[] = "dst_addr";

constexpr char HDR_TCPUDP_SRC_PORT_FIELD[] = "src_port";
constexpr char HDR_TCPUDP_DST_PORT_FIELD[] = "dst_port";

} // namespace tofino
} // namespace synthesizer
} // namespace synapse