#ifdef __cplusplus
extern "C" {
#endif
#include <lib/verified/cht.h>
#include <lib/verified/double-chain.h>
#include <lib/verified/map.h>
#include <lib/verified/vector.h>
#include <lib/unverified/sketch.h>
#include <lib/unverified/util.h>

#include <lib/verified/expirator.h>
#include <lib/verified/packet-io.h>
#include <lib/verified/tcpudp_hdr.h>
#include <lib/verified/vigor-time.h>
#ifdef __cplusplus
}
#endif

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_random.h>

#include <getopt.h>
#include <pcap.h>
#include <stdbool.h>
#include <unistd.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

#define REPORT_NAME "bdd-analysis"

#define NF_INFO(text, ...)                                                     \
  printf(text "\n", ##__VA_ARGS__);                                            \
  fflush(stdout);

#ifdef ENABLE_LOG
#define NF_DEBUG(text, ...)                                                    \
  fprintf(stderr, "DEBUG: " text "\n", ##__VA_ARGS__);                         \
  fflush(stderr);
#else // ENABLE_LOG
#define NF_DEBUG(...)
#endif // ENABLE_LOG

#define MIN_PKT_SIZE 64   // With CRC
#define MAX_PKT_SIZE 1518 // With CRC

#define ARG_HELP "help"
#define ARG_DEVICE "device"
#define ARG_PCAP "pcap"
#define ARG_TOTAL_PACKETS "packets"
#define ARG_TOTAL_FLOWS "flows"
#define ARG_TOTAL_CHURN_FPM "churn"
#define ARG_TOTAL_RATE_PPS "pps"
#define ARG_PACKET_SIZES "size"
#define ARG_TRAFFIC_UNIFORM "uniform"
#define ARG_TRAFFIC_ZIPF "zipf"
#define ARG_TRAFFIC_ZIPF_PARAM "zipf-param"
#define ARG_RANDOM_SEED "seed"

#define DEFAULT_DEVICE 0
#define DEFAULT_TOTAL_PACKETS 1000000lu
#define DEFAULT_TOTAL_FLOWS 65536lu
#define DEFAULT_TOTAL_CHURN_FPM 0lu
#define DEFAULT_TOTAL_RATE_PPS 150'000'000lu
#define DEFAULT_PACKET_SIZES MIN_PKT_SIZE
#define DEFAULT_TRAFFIC_UNIFORM true
#define DEFAULT_TRAFFIC_ZIPF false
#define DEFAULT_TRAFFIC_ZIPF_PARAMETER 1.26 // From Castan [SIGCOMM'18]

#define PARSE_ERROR(argv, format, ...)                                         \
  nf_config_usage(argv);                                                       \
  fprintf(stderr, format, ##__VA_ARGS__);                                      \
  exit(EXIT_FAILURE);

#define PARSER_ASSERT(cond, fmt, ...)                                          \
  if (!(cond))                                                                 \
    rte_exit(EXIT_FAILURE, fmt, ##__VA_ARGS__);

enum {
  /* long options mapped to short options: first long only option value must
   * be >= 256, so that it does not conflict with short options.
   */
  ARG_HELP_NUM = 256,
  ARG_DEVICE_NUM,
  ARG_PCAP_NUM,
  ARG_TOTAL_PACKETS_NUM,
  ARG_TOTAL_FLOWS_NUM,
  ARG_TOTAL_CHURN_FPM_NUM,
  ARG_TOTAL_RATE_PPS_NUM,
  ARG_PACKET_SIZES_NUM,
  ARG_TRAFFIC_UNIFORM_NUM,
  ARG_TRAFFIC_ZIPF_NUM,
  ARG_TRAFFIC_ZIPF_PARAM_NUM,
  ARG_RANDOM_SEED_NUM,
};

bool nf_init(void);
int nf_process(uint16_t device, uint8_t *buffer, uint16_t packet_length,
               time_ns_t now);

uintmax_t nf_util_parse_int(const char *str, const char *name, int base,
                            char next) {
  char *temp;
  intmax_t result = strtoimax(str, &temp, base);

  // There's also a weird failure case with overflows, but let's not care
  if (temp == str || *temp != next) {
    rte_exit(EXIT_FAILURE, "Error while parsing '%s': %s\n", name, str);
  }

  return result;
}

struct flow_t {
  uint32_t src_ip;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port;
};

flow_t generate_random_flow() {
  flow_t flow;
  flow.src_ip = rte_rand();
  flow.dst_ip = rte_rand();
  flow.src_port = rte_rand();
  flow.dst_port = rte_rand();
  return flow;
}

struct pkt_t {
  uint8_t data[MAX_PKT_SIZE];
  uint32_t len;
  time_ns_t ts;
};

struct config_t {
  std::string nf_name;
  uint16_t device;
  const char *pcap;
  uint64_t total_packets;
  uint64_t total_flows;
  uint64_t churn_fpm;
  uint64_t rate_pps;
  uint16_t packet_sizes;
  bool traffic_uniform;
  bool traffic_zipf;
  float traffic_zipf_param;
} config;

class PacketGenerator {
private:
  config_t config;

  pcap_t *pcap;
  long pcap_start;

  uint64_t total_packets;
  uint64_t generated_packets;
  time_ns_t last_time;
  time_ns_t churn_alarm;
  time_ns_t churn_alarm_delta;
  std::vector<flow_t> flows;
  int last_percentage_report;

public:
  PacketGenerator(const config_t &_config)
      : config(_config), pcap(NULL), total_packets(config.total_packets),
        generated_packets(0), last_time(0),
        churn_alarm_delta(
            config.churn_fpm == 0 ? 0 : ((1e9 * 60) / config.churn_fpm)),
        churn_alarm(0), last_percentage_report(-1) {
    if (!config.pcap) {
      return;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap = pcap_open_offline(config.pcap, errbuf);

    if (pcap == NULL) {
      rte_exit(EXIT_FAILURE, "pcap_open_offline() failed: %s\n", errbuf);
    }

    // Get the total number of packets inside the pcap file first.
    FILE *pcap_fptr = pcap_file(pcap);
    assert(pcap_fptr && "Invalid pcap file pointer");
    pcap_start = ftell(pcap_fptr);

    struct pcap_pkthdr *hdr;
    const u_char *pcap_pkt;
    int success;
    total_packets = 0;
    while ((success = pcap_next_ex(pcap, &hdr, &pcap_pkt)) == 1) {
      total_packets++;
    }

    // Then rewind.
    fseek(pcap_fptr, pcap_start, SEEK_SET);
  }

  bool generate(pkt_t &pkt) {
    return pcap ? generate_with_pcap(pkt) : generate_without_pcap(pkt);
  }

private:
  bool generate_with_pcap(pkt_t &pkt) {
    if (generated_packets >= total_packets) {
      return false;
    }

    struct pcap_pkthdr *hdr;
    const u_char *pcap_pkt;

    int success = pcap_next_ex(pcap, &hdr, &pcap_pkt);
    assert(success >= 0 && "Error reading pcap file");

    memcpy(pkt.data, pcap_pkt, hdr->len);
    pkt.len = hdr->len;
    pkt.ts = hdr->ts.tv_sec * 1e9 + hdr->ts.tv_usec * 1e3;

    update_and_show_progress();
    return true;
  }

  bool generate_without_pcap(pkt_t &pkt) {
    if (generated_packets >= total_packets) {
      return false;
    }

    const flow_t &flow = get_next_flow();

    // Generate a packet
    pkt.len = config.packet_sizes;
    pkt.ts = last_time + 1e9 / config.rate_pps;

    last_time = pkt.ts;

    struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *)pkt.data;
    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);

    eth_hdr->ether_type = rte_bswap16(RTE_ETHER_TYPE_IPV4);

    ip_hdr->version_ihl = RTE_IPV4_VHL_DEF;
    ip_hdr->total_length = rte_bswap16(pkt.len - sizeof(struct rte_ether_hdr));
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->src_addr = flow.src_ip;
    ip_hdr->dst_addr = flow.dst_ip;

    udp_hdr->src_port = flow.src_port;
    udp_hdr->dst_port = flow.dst_port;

    update_and_show_progress();
    return true;
  }

  void update_and_show_progress() {
    generated_packets++;
    int percentage = 100 * generated_packets / (double)total_packets;

    if (percentage == last_percentage_report) {
      return;
    }

    last_percentage_report = percentage;
    printf("\rProcessing packets %lu/%lu (%d%%) ...", generated_packets,
           total_packets, percentage);

    if (generated_packets == total_packets) {
      printf("\n");
    }

    fflush(stdout);
  }

  const flow_t &get_next_flow() {
    if (config.traffic_uniform) {
      if (generated_packets < config.total_flows) {
        flow_t random_flow = generate_random_flow();
        flows.push_back(random_flow);
        return flows[generated_packets];
      }

      int curr_flow_i = generated_packets % config.total_flows;

      if (churn_alarm_delta > 0 && last_time >= churn_alarm) {
        flows[curr_flow_i] = generate_random_flow();
        churn_alarm = last_time + churn_alarm_delta;
      }

      return flows[curr_flow_i];
    }

    assert(false && "Zipf traffic not implemented");
  }
};

void nf_log_pkt(time_ns_t time, uint16_t device, uint8_t *packet,
                uint16_t packet_length) {
  struct rte_ether_hdr *rte_ether_header = (struct rte_ether_hdr *)(packet);
  struct rte_ipv4_hdr *rte_ipv4_header =
      (struct rte_ipv4_hdr *)(packet + sizeof(struct rte_ether_hdr));
  struct tcpudp_hdr *tcpudp_header =
      (struct tcpudp_hdr *)(packet + sizeof(struct rte_ether_hdr) +
                            sizeof(struct rte_ipv4_hdr));

  NF_DEBUG("[%lu:%u] %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u", time, device,
           (rte_ipv4_header->src_addr >> 0) & 0xff,
           (rte_ipv4_header->src_addr >> 8) & 0xff,
           (rte_ipv4_header->src_addr >> 16) & 0xff,
           (rte_ipv4_header->src_addr >> 24) & 0xff,
           rte_bswap16(tcpudp_header->src_port),
           (rte_ipv4_header->dst_addr >> 0) & 0xff,
           (rte_ipv4_header->dst_addr >> 8) & 0xff,
           (rte_ipv4_header->dst_addr >> 16) & 0xff,
           (rte_ipv4_header->dst_addr >> 24) & 0xff,
           rte_bswap16(tcpudp_header->dst_port));
}

void nf_config_usage(char **argv) {
  NF_INFO(
      "Usage: %s\n"
      "\t [--" ARG_DEVICE " <dev> (default=%u)]\n"
      "\t [--" ARG_PCAP " <pcap>]\n"
      "\t [--" ARG_TOTAL_PACKETS " <#packets> (default=%lu)]\n"
      "\t [--" ARG_TOTAL_FLOWS " <#flows> (default=%lu)]\n"
      "\t [--" ARG_TOTAL_CHURN_FPM " <fpm> (default=%lu)]\n"
      "\t [--" ARG_TOTAL_RATE_PPS " <pps> (default=%lu)]\n"
      "\t [--" ARG_PACKET_SIZES " <bytes> (default=%u)]\n"
      "\t [--" ARG_TRAFFIC_UNIFORM " (default=%s)]\n"
      "\t [--" ARG_TRAFFIC_ZIPF " (default=%s)]\n"
      "\t [--" ARG_TRAFFIC_ZIPF_PARAM " <param> (default=%f)]\n"
      "\t [--" ARG_RANDOM_SEED " <seed> (default set by DPDK)]\n"
      "\t [--" ARG_HELP "]\n"
      "\n"
      "Argument descriptions:\n"
      "\t " ARG_DEVICE ": networking device to be analyzed\n"
      "\t " ARG_PCAP ": traffic trace to analyze (using this argument makes "
      "the program ignore all the other ones, as those are extracted "
      "directly from the pcap)\n"
      "\t " ARG_TOTAL_PACKETS ": total number of packets to generate\n"
      "\t " ARG_TOTAL_FLOWS ": total number of flows to generate\n"
      "\t " ARG_TOTAL_CHURN_FPM ": flow churn (fpm)\n"
      "\t " ARG_TOTAL_RATE_PPS ": packet rate (pps)\n"
      "\t " ARG_PACKET_SIZES ": packet sizes (bytes)\n"
      "\t " ARG_RANDOM_SEED ": random seed\n"
      "\t " ARG_HELP ": show this menu\n",
      argv[0], DEFAULT_DEVICE, DEFAULT_TOTAL_PACKETS, DEFAULT_TOTAL_FLOWS,
      DEFAULT_TOTAL_CHURN_FPM, DEFAULT_TOTAL_RATE_PPS, DEFAULT_PACKET_SIZES,
      DEFAULT_TRAFFIC_UNIFORM ? "true" : "false",
      DEFAULT_TRAFFIC_ZIPF ? "true" : "false", DEFAULT_TRAFFIC_ZIPF_PARAMETER);
}

void nf_config_print(void) {
  NF_INFO("----- Config -----");
  NF_INFO("device:     %u", config.device);
  if (config.pcap) {
    NF_INFO("pcap:       %s", config.pcap);
  } else {
    NF_INFO("#packets:   %lu", config.total_packets);
    NF_INFO("#flows:     %lu", config.total_flows);
    NF_INFO("churn:      %lu fpm", config.churn_fpm);
    NF_INFO("rate:       %lu pps", config.rate_pps);
    NF_INFO("pkt sizes:  %u bytes", config.packet_sizes);
    if (config.traffic_uniform) {
      NF_INFO("traffic:    uniform");
    } else if (config.traffic_zipf) {
      NF_INFO("traffic:    zipf");
      NF_INFO("zipf param: %f", config.traffic_zipf_param);
    }
  }
  NF_INFO("--- ---------- ---");
}

std::string nf_name_from_executable(const char *argv0) {
  std::string nf_name = argv0;
  size_t last_slash = nf_name.find_last_of('/');
  if (last_slash != std::string::npos) {
    nf_name = nf_name.substr(last_slash + 1);
  }
  return nf_name;
}

void nf_config_init(int argc, char **argv) {
  config.nf_name = nf_name_from_executable(argv[0]);
  config.device = DEFAULT_DEVICE;
  config.pcap = NULL;
  config.total_packets = DEFAULT_TOTAL_PACKETS;
  config.total_flows = DEFAULT_TOTAL_FLOWS;
  config.churn_fpm = DEFAULT_TOTAL_CHURN_FPM;
  config.rate_pps = DEFAULT_TOTAL_RATE_PPS;
  config.packet_sizes = DEFAULT_PACKET_SIZES;
  config.traffic_uniform = DEFAULT_TRAFFIC_UNIFORM;
  config.traffic_zipf = DEFAULT_TRAFFIC_ZIPF;
  config.traffic_zipf_param = DEFAULT_TRAFFIC_ZIPF_PARAMETER;

  const char short_options[] = "";

  struct option long_options[] = {
      {ARG_DEVICE, required_argument, NULL, ARG_DEVICE_NUM},
      {ARG_PCAP, required_argument, NULL, ARG_PCAP_NUM},
      {ARG_TOTAL_PACKETS, required_argument, NULL, ARG_TOTAL_PACKETS_NUM},
      {ARG_TOTAL_FLOWS, required_argument, NULL, ARG_TOTAL_FLOWS_NUM},
      {ARG_TOTAL_CHURN_FPM, required_argument, NULL, ARG_TOTAL_CHURN_FPM_NUM},
      {ARG_TOTAL_RATE_PPS, required_argument, NULL, ARG_TOTAL_RATE_PPS_NUM},
      {ARG_PACKET_SIZES, required_argument, NULL, ARG_PACKET_SIZES_NUM},
      {ARG_TRAFFIC_UNIFORM, no_argument, NULL, ARG_TRAFFIC_UNIFORM_NUM},
      {ARG_TRAFFIC_ZIPF, no_argument, NULL, ARG_TRAFFIC_ZIPF_NUM},
      {ARG_TRAFFIC_ZIPF_PARAM, required_argument, NULL,
       ARG_TRAFFIC_ZIPF_PARAM_NUM},
      {ARG_RANDOM_SEED, required_argument, NULL, ARG_RANDOM_SEED_NUM},
      {ARG_HELP, no_argument, NULL, ARG_HELP_NUM},
      {NULL, 0, NULL, 0}};

  int opt;

  while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) !=
         EOF) {
    switch (opt) {
    case ARG_HELP_NUM: {
      nf_config_usage(argv);
      exit(EXIT_SUCCESS);
    } break;
    case ARG_DEVICE_NUM: {
      config.device = nf_util_parse_int(optarg, "device", 10, '\0');
    } break;
    case ARG_PCAP_NUM: {
      config.pcap = optarg;
    } break;
    case ARG_TOTAL_PACKETS_NUM: {
      config.total_packets = nf_util_parse_int(optarg, "packets", 10, '\0');
    } break;
    case ARG_TOTAL_FLOWS_NUM: {
      config.total_flows = nf_util_parse_int(optarg, "flows", 10, '\0');
    } break;
    case ARG_TOTAL_CHURN_FPM_NUM: {
      config.churn_fpm = nf_util_parse_int(optarg, "churn", 10, '\0');
    } break;
    case ARG_TOTAL_RATE_PPS_NUM: {
      config.rate_pps = nf_util_parse_int(optarg, "pps", 10, '\0');
    } break;
    case ARG_PACKET_SIZES_NUM: {
      config.packet_sizes = nf_util_parse_int(optarg, "size", 10, '\0');
      PARSER_ASSERT(config.packet_sizes >= MIN_PKT_SIZE &&
                        config.packet_sizes <= MAX_PKT_SIZE,
                    "Packet size must be in the interval [%u-%" PRIu16
                    "] (requested %u).\n",
                    MIN_PKT_SIZE, MAX_PKT_SIZE, config.packet_sizes);
    } break;
    case ARG_TRAFFIC_UNIFORM_NUM: {
      config.traffic_uniform = true;
      config.traffic_zipf = false;
    } break;
    case ARG_TRAFFIC_ZIPF_NUM: {
      config.traffic_uniform = false;
      config.traffic_zipf = true;
    } break;
    case ARG_TRAFFIC_ZIPF_PARAM_NUM: {
      config.traffic_zipf_param = strtof(optarg, NULL);
    } break;
    case ARG_RANDOM_SEED_NUM: {
      uint32_t seed = nf_util_parse_int(optarg, "seed", 10, '\0');
      rte_srand(seed);
    } break;
    default:
      PARSE_ERROR(argv, "Unknown option.\n");
    }
  }

  nf_config_print();
}

uint64_t *bdd_node_hit_counter_ptr;
uint64_t bdd_node_hit_counter_sz;
time_ns_t elapsed_time;

void generate_report() {
  json report;

  report["config"] = json();
  report["config"]["device"] = config.device;
  if (config.pcap) {
    report["config"]["pcap"] = config.pcap;
  } else {
    report["config"]["total_packets"] = config.total_packets;
    report["config"]["total_flows"] = config.total_flows;
    report["config"]["churn_fpm"] = config.churn_fpm;
    report["config"]["rate_pps"] = config.rate_pps;
    report["config"]["packet_sizes"] = config.packet_sizes;
    report["config"]["traffic_uniform"] = config.traffic_uniform;
    report["config"]["traffic_zipf"] = config.traffic_zipf;
    report["config"]["traffic_zipf_param"] = config.traffic_zipf_param;
  }

  report["elapsed"] = elapsed_time;

  report["counters"] = json();
  for (unsigned i = 0; i < bdd_node_hit_counter_sz; i++) {
    report["counters"][std::to_string(i)] = bdd_node_hit_counter_ptr[i];
  }

  std::stringstream report_fname_ss;
  report_fname_ss << config.nf_name;
  report_fname_ss << "-" << REPORT_NAME;
  report_fname_ss << "-dev-" << config.device;
  if (config.pcap) {
    report_fname_ss << "-pcap-" << config.pcap;
  } else {
    report_fname_ss << "-packets-" << config.total_packets;
    report_fname_ss << "-flows-" << config.total_flows;
    report_fname_ss << "-churn-" << config.churn_fpm;
    report_fname_ss << "-rate-" << config.rate_pps;
    report_fname_ss << "-size-" << config.packet_sizes;
    if (config.traffic_uniform) {
      report_fname_ss << "-uniform";
    } else if (config.traffic_zipf) {
      report_fname_ss << "-zipf-param-" << config.traffic_zipf_param;
    }
  }
  report_fname_ss << ".json";

  auto report_fname = report_fname_ss.str();
  std::ofstream(report_fname) << report.dump(2);

  NF_INFO("Generated report %s", report_fname.c_str());
}

// Main worker method (for now used on a single thread...)
static void worker_main() {
  if (!nf_init()) {
    rte_exit(EXIT_FAILURE, "Error initializing NF");
  }

  PacketGenerator pkt_gen(config);
  pkt_t pkt;

  // Generate the first packet manually to record the starting time
  bool success = pkt_gen.generate(pkt);
  assert(success && "Failed to generate the first packet");

  time_ns_t start_time = pkt.ts;
  time_ns_t last_time = 0;

  do {
    // Ignore destination device, we don't forward anywhere
    nf_process(config.device, pkt.data, pkt.len, pkt.ts);
    last_time = pkt.ts;
  } while (pkt_gen.generate(pkt));

  elapsed_time = last_time - start_time;
  NF_INFO("Elapsed time: %lf s", (double)elapsed_time / 1e9);
}

int main(int argc, char **argv) {
  nf_config_init(argc, argv);
  worker_main();
  generate_report();
  return 0;
}
