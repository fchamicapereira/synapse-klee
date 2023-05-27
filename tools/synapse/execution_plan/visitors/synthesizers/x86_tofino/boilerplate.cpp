#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <bf_rt/bf_rt.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_table.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table_key.hpp>
#include <port_mgr/bf_port_if.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <bf_rt/bf_rt_common.h>
#include <bf_switchd/bf_switchd.h>
#include <bfutils/bf_utils.h> // required for bfshell
#include <pkt_mgr/pkt_mgr_intf.h>
#ifdef __cplusplus
}
#endif

#define ALL_PIPES 0xffff

#define ENV_VAR_SDE_INSTALL "SDE_INSTALL"
#define PROGRAM_NAME "tofino"

#define SWITCH_PACKET_MAX_BUFFER_SIZE 10000
#define MTU 1500

#define SWAP_ENDIAN_16(v)                                                      \
  { (v) = __bswap_16((v)); }
#define SWAP_ENDIAN_32(v)                                                      \
  { (v) = __bswap_32((v)); }

typedef size_t bits_t;

bf_rt_target_t dev_tgt;
const bfrt::BfRtInfo *info;
std::shared_ptr<bfrt::BfRtSession> session;

struct cpu_t {
  uint16_t code_path;
  uint16_t in_port;
  uint16_t out_port;
  /*@{CPU HEADER DATAPLANE STATE FIELDS}@*/  

  void dump() const {
    printf("###[ CPU ]###\n");
    printf("code path  %u\n", code_path);
    printf("in port    %u\n", in_port);
    printf("out port   %u\n", out_port);
  }
} __attribute__((packed));

struct eth_t {
  uint8_t dst_mac[6];
  uint8_t src_mac[6];
  uint16_t eth_type;

  void dump() const {
    printf("###[ Ethernet ]###\n");
    printf("dst  %02x:%02x:%02x:%02x:%02x:%02x\n", dst_mac[0], dst_mac[1],
           dst_mac[2], dst_mac[3], dst_mac[4], dst_mac[5]);
    printf("src  %02x:%02x:%02x:%02x:%02x:%02x\n", src_mac[0], src_mac[1],
           src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
    printf("type 0x%x\n", eth_type);
  }
} __attribute__((packed));

struct ipv4_t {
  uint8_t version_ihl;
  uint8_t ecn_dscp;
  uint16_t tot_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t check;
  uint32_t src_ip;
  uint32_t dst_ip;

  void dump() const {
    printf("###[ IP ]###\n");
    printf("version %u\n", (version_ihl & 0xf0) >> 4);
    printf("ihl     %u\n", (version_ihl & 0x0f));
    printf("tos     %u\n", ecn_dscp);
    printf("len     %u\n", ntohs(tot_len));
    printf("id      %u\n", ntohs(id));
    printf("off     %u\n", ntohs(frag_off));
    printf("ttl     %u\n", ttl);
    printf("proto   %u\n", protocol);
    printf("chksum  0x%x\n", ntohs(check));
    printf("src     %u.%u.%u.%u\n", (src_ip >> 0) & 0xff, (src_ip >> 8) & 0xff,
           (src_ip >> 16) & 0xff, (src_ip >> 24) & 0xff);
    printf("dst     %u.%u.%u.%u\n", (dst_ip >> 0) & 0xff, (dst_ip >> 8) & 0xff,
           (dst_ip >> 16) & 0xff, (dst_ip >> 24) & 0xff);
  }
} __attribute__((packed));

struct tcpudp_t {
  uint16_t src_port;
  uint16_t dst_port;

  void dump() const {
    printf("###[ TCP/UDP ]###\n");
    printf("sport   %u\n", ntohs(src_port));
    printf("dport   %u\n", ntohs(dst_port));
  }
} __attribute__((packed));

struct pkt_t {
  uint8_t *base;
  uint8_t *curr;
  uint32_t size;

  pkt_t(uint8_t *_data, uint32_t _size)
      : base(_data), curr(_data), size(_size) {}

  cpu_t *parse_cpu() {
    auto ptr = (cpu_t *)curr;
    curr += sizeof(cpu_t);

    SWAP_ENDIAN_16(ptr->code_path);
    SWAP_ENDIAN_16(ptr->in_port);
    SWAP_ENDIAN_16(ptr->out_port);

    return ptr;
  }

  eth_t *parse_ethernet() {
    auto ptr = (eth_t *)curr;
    curr += sizeof(eth_t);
    return ptr;
  }

  ipv4_t *parse_ipv4() {
    auto ptr = curr;
    curr += sizeof(ipv4_t);
    return (ipv4_t *)ptr;
  }

  tcpudp_t *parse_tcpudp() {
    auto ptr = curr;
    curr += sizeof(tcpudp_t);
    return (tcpudp_t *)ptr;
  }

  void commit() {
    auto cpu = (cpu_t *)base;
    SWAP_ENDIAN_16(cpu->code_path);
    SWAP_ENDIAN_16(cpu->in_port);
    SWAP_ENDIAN_16(cpu->out_port);
  }
};

unsigned ether_addr_hash(uint8_t *addr) {
  uint8_t addr_bytes_0 = addr[0];
  uint8_t addr_bytes_1 = addr[1];
  uint8_t addr_bytes_2 = addr[2];
  uint8_t addr_bytes_3 = addr[3];
  uint8_t addr_bytes_4 = addr[4];
  uint8_t addr_bytes_5 = addr[5];

  unsigned hash = 0;
  hash = __builtin_ia32_crc32si(hash, addr_bytes_0);
  hash = __builtin_ia32_crc32si(hash, addr_bytes_1);
  hash = __builtin_ia32_crc32si(hash, addr_bytes_2);
  hash = __builtin_ia32_crc32si(hash, addr_bytes_3);
  hash = __builtin_ia32_crc32si(hash, addr_bytes_4);
  hash = __builtin_ia32_crc32si(hash, addr_bytes_5);
  return hash;
}

uint32_t __raw_cksum(const void *buf, size_t len, uint32_t sum) {
  /* workaround gcc strict-aliasing warning */
  uintptr_t ptr = (uintptr_t)buf;
  typedef uint16_t __attribute__((__may_alias__)) u16_p;
  const u16_p *u16_buf = (const u16_p *)ptr;

  while (len >= (sizeof(*u16_buf) * 4)) {
    sum += u16_buf[0];
    sum += u16_buf[1];
    sum += u16_buf[2];
    sum += u16_buf[3];
    len -= sizeof(*u16_buf) * 4;
    u16_buf += 4;
  }
  while (len >= sizeof(*u16_buf)) {
    sum += *u16_buf;
    len -= sizeof(*u16_buf);
    u16_buf += 1;
  }

  /* if length is in odd bytes */
  if (len == 1) {
    uint16_t left = 0;
    *(uint8_t *)&left = *(const uint8_t *)u16_buf;
    sum += left;
  }

  return sum;
}

uint16_t __raw_cksum_reduce(uint32_t sum) {
  sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
  sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
  return (uint16_t)sum;
}

uint16_t raw_cksum(const void *buf, size_t len) {
  uint32_t sum;

  sum = __raw_cksum(buf, len, 0);
  return __raw_cksum_reduce(sum);
}

uint16_t ipv4_cksum(const ipv4_t *ipv4_hdr) {
  uint16_t cksum;
  cksum = raw_cksum(ipv4_hdr, sizeof(ipv4_t));
  return (uint16_t)~cksum;
}

uint16_t update_ipv4_tcpudp_checksums(const ipv4_t *ipv4_hdr,
                                      const void *l4_hdr) {
  uint32_t cksum;
  uint32_t l3_len, l4_len;

  l3_len = __bswap_16(ipv4_hdr->tot_len);
  if (l3_len < sizeof(ipv4_t))
    return 0;

  l4_len = l3_len - sizeof(ipv4_t);

  cksum = raw_cksum(l4_hdr, l4_len);
  cksum += ipv4_cksum(ipv4_hdr);

  cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
  cksum = (~cksum) & 0xffff;
  /*
   * Per RFC 768:If the computed checksum is zero for UDP,
   * it is transmitted as all ones
   * (the equivalent in one's complement arithmetic).
   */
  if (cksum == 0 && ipv4_hdr->protocol == IPPROTO_UDP)
    cksum = 0xffff;

  return (uint16_t)cksum;
}

char *get_env_var_value(const char *env_var) {
  auto env_var_value = getenv(env_var);

  if (!env_var_value) {
    std::cerr << env_var << " env var not found.\n";
    exit(1);
  }

  return env_var_value;
}

char *get_install_dir() { return get_env_var_value(ENV_VAR_SDE_INSTALL); }

typedef uint64_t time_ns_t;

time_ns_t get_time() {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec * 1000000000ul + tp.tv_nsec;
}

typedef uint16_t port_t;

constexpr port_t DROP = 0xffff;
constexpr port_t BCAST = 0xfffe;

bool nf_process(time_ns_t now, pkt_t &pkt);

void pcie_tx(bf_dev_id_t device, uint8_t *pkt, uint32_t packet_size) {
  bf_pkt *tx_pkt = nullptr;

  if (bf_pkt_alloc(dev_tgt.dev_id, &tx_pkt, packet_size,
                   BF_DMA_CPU_PKT_TRANSMIT_0) != 0) {
    fprintf(stderr, "Failed to allocate packet buffer\n");
    exit(1);
  }

  if (bf_pkt_data_copy(tx_pkt, pkt, packet_size) != 0) {
    fprintf(stderr, "bf_pkt_data_copy failed: pkt_size=%d\n", packet_size);
    bf_pkt_free(device, tx_pkt);
    return;
  }

  if (bf_pkt_tx(device, tx_pkt, BF_PKT_TX_RING_0, (void *)tx_pkt) !=
      BF_SUCCESS) {
    fprintf(stderr, "bf_pkt_tx failed\n");
    bf_pkt_free(device, tx_pkt);
  }
}

bf_status_t txComplete(bf_dev_id_t device, bf_pkt_tx_ring_t tx_ring,
                       uint64_t tx_cookie, uint32_t status) {
  return BF_SUCCESS;
}

bf_status_t pcie_rx(bf_dev_id_t device, bf_pkt *pkt, void *data,
                    bf_pkt_rx_ring_t rx_ring) {
  bf_pkt *orig_pkt = nullptr;
  char in_packet[SWITCH_PACKET_MAX_BUFFER_SIZE];
  char *pkt_buf = nullptr;
  char *bufp = nullptr;
  uint32_t packet_size = 0;
  uint16_t pkt_len = 0;

  // save a pointer to the packet
  orig_pkt = pkt;

  // assemble the received packet
  bufp = &in_packet[0];

  do {
    pkt_buf = (char *)bf_pkt_get_pkt_data(pkt);
    pkt_len = bf_pkt_get_pkt_size(pkt);

    if ((packet_size + pkt_len) > SWITCH_PACKET_MAX_BUFFER_SIZE) {
      fprintf(stderr, "Packet too large to Transmit - SKipping\n");
      break;
    }

    memcpy(bufp, pkt_buf, pkt_len);
    bufp += pkt_len;
    packet_size += pkt_len;
    pkt = bf_pkt_get_nextseg(pkt);
  } while (pkt);

  auto atomic = true;
  auto hw_sync = true;

  auto now = get_time();
  auto p = pkt_t((uint8_t *)in_packet, packet_size);

  session->beginTransaction(atomic);
  auto fwd = nf_process(now, p);
  session->commitTransaction(hw_sync);

  if (fwd) {
    p.commit();
    pcie_tx(device, (uint8_t *)in_packet, packet_size);
  }

  bf_pkt_free(device, orig_pkt);

  return BF_SUCCESS;
}

void register_pcie_pkt_ops() {
  // register callback for TX complete
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       tx_ring++) {
    bf_pkt_tx_done_notif_register(dev_tgt.dev_id, txComplete,
                                  (bf_pkt_tx_ring_t)tx_ring);
  }

  // register callback for RX
  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       rx_ring++) {
    auto status = bf_pkt_rx_register(dev_tgt.dev_id, pcie_rx,
                                     (bf_pkt_rx_ring_t)rx_ring, 0);
    if (status != BF_SUCCESS) {
      fprintf(stderr, "Failed to register pcie callback\n");
      exit(1);
    }
  }
}

void init_bf_switchd() {
  auto switchd_main_ctx =
      (bf_switchd_context_t *)calloc(1, sizeof(bf_switchd_context_t));

  /* Allocate memory to hold switchd configuration and state */
  if (switchd_main_ctx == NULL) {
    std::cerr << "ERROR: Failed to allocate memory for switchd context\n";
    exit(1);
  }

  char target_conf_file[100];
  sprintf(target_conf_file, "%s/share/p4/targets/tofino/%s.conf",
          get_install_dir(), PROGRAM_NAME);

  memset(switchd_main_ctx, 0, sizeof(bf_switchd_context_t));

  switchd_main_ctx->install_dir = get_install_dir();
  switchd_main_ctx->conf_file = const_cast<char *>(target_conf_file);
  switchd_main_ctx->skip_p4 = false;
  switchd_main_ctx->skip_port_add = false;
  switchd_main_ctx->running_in_background = false;
  switchd_main_ctx->dev_sts_thread = false;

  auto bf_status = bf_switchd_lib_init(switchd_main_ctx);

  if (bf_status != BF_SUCCESS) {
    exit(1);
  }
}

void setup_bf_session() {
  dev_tgt.dev_id = 0;
  dev_tgt.pipe_id = ALL_PIPES;

  // Get devMgr singleton instance
  auto &devMgr = bfrt::BfRtDevMgr::getInstance();

  // Get info object from dev_id and p4 program name
  auto bf_status = devMgr.bfRtInfoGet(dev_tgt.dev_id, PROGRAM_NAME, &info);
  assert(bf_status == BF_SUCCESS);

  // Create a session object
  session = bfrt::BfRtSession::sessionCreate();
}

template <int key_size> bool key_eq(void *k1, void *k2) {
  auto _k1 = (uint8_t *)k1;
  auto _k2 = (uint8_t *)k2;

  for (auto i = 0; i < key_size; i++) {
    if (_k1[i] != _k2[i]) {
      return false;
    }
  }

  return true;
}

template <int key_size> unsigned key_hash(void *k) {
  auto _k = (uint8_t *)k;
  unsigned hash = 0;

  for (auto i = 0; i < key_size; i++) {
    hash = __builtin_ia32_crc32si(hash, _k[i]);
  }

  return hash;
}

class __Table {
protected:
  std::string table_name;
  const bfrt::BfRtTable *table;

  std::unique_ptr<bfrt::BfRtTableKey> key;
  std::unique_ptr<bfrt::BfRtTableData> data;

  struct field_t {
    std::string name;
    bf_rt_id_t id;
    bits_t size;
  };

  std::vector<field_t> key_fields;
  std::vector<field_t> data_fields;

protected:
  __Table(const std::string &_table_name)
      : table_name(_table_name), table(nullptr) {
    assert(info);
    assert(session);

    auto bf_status = info->bfrtTableFromNameGet(table_name, &table);
    assert(bf_status == BF_SUCCESS);

    // Allocate key and data once, and use reset across different uses
    bf_status = table->keyAllocate(&key);
    assert(bf_status == BF_SUCCESS);
    assert(key);

    bf_status = table->dataAllocate(&data);
    assert(bf_status == BF_SUCCESS);
    assert(data);
  }

  void init_key() {
    std::vector<bf_rt_id_t> key_fields_ids;

    auto bf_status = table->keyFieldIdListGet(&key_fields_ids);
    assert(bf_status == BF_SUCCESS);

    for (auto id : key_fields_ids) {
      std::string name;
      bits_t size;

      bf_status = table->keyFieldNameGet(id, &name);
      assert(bf_status == BF_SUCCESS);

      bf_status = table->keyFieldSizeGet(id, &size);
      assert(bf_status == BF_SUCCESS);

      key_fields.push_back({name, id, size});
    }

    std::cerr << "Keys:\n";
    for (auto k : key_fields) {
      std::cerr << "  " << k.name << " (" << k.size << " bits)\n";
    }
  }

  void init_key(const std::string &name, bf_rt_id_t *id) {
    auto bf_status = table->keyFieldIdGet(name, id);

    if (bf_status != BF_SUCCESS) {
      std::cerr << "Error configuring table key " << name << "\n";
      exit(1);
    }
  }

  void init_key(std::unordered_map<std::string, bf_rt_id_t *> fields) {
    for (const auto &field : fields) {
      init_key(field.first, field.second);
    }
  }

  void init_data(const std::string &name, bf_rt_id_t *id) {
    auto bf_status = table->dataFieldIdGet(name, id);

    if (bf_status != BF_SUCCESS) {
      std::cerr << "Error configuring table data " << name << "\n";
      exit(1);
    }
  }

  void init_data(std::unordered_map<std::string, bf_rt_id_t *> fields) {
    for (const auto &field : fields) {
      init_data(field.first, field.second);
    }
  }

  void init_data_with_action(bf_rt_id_t action_id) {
    std::vector<bf_rt_id_t> data_fields_ids;

    auto bf_status = table->dataFieldIdListGet(action_id, &data_fields_ids);
    assert(bf_status == BF_SUCCESS);

    for (auto id : data_fields_ids) {
      std::string name;
      bits_t size;

      bf_status = table->dataFieldNameGet(id, action_id, &name);
      assert(bf_status == BF_SUCCESS);

      bf_status = table->dataFieldSizeGet(id, action_id, &size);
      assert(bf_status == BF_SUCCESS);

      data_fields.push_back({name, id, size});
    }

    std::cerr << "Data:\n";
    for (auto d : data_fields) {
      std::cerr << "  " << d.name << " (" << d.size << " bits)\n";
    }
  }

  void init_data_with_action(const std::string &name, bf_rt_id_t action_id,
                             bf_rt_id_t *field_id) {
    auto bf_status = table->dataFieldIdGet(name, action_id, field_id);

    if (bf_status != BF_SUCCESS) {
      std::cerr << "Error configuring table data with actions " << name << "\n";
      exit(1);
    }
  }

  void init_data_with_actions(
      std::unordered_map<std::string, std::pair<bf_rt_id_t, bf_rt_id_t *>>
          fields) {
    for (const auto &field : fields) {
      init_data_with_action(field.first, field.second.first,
                            field.second.second);
    }
  }

  void init_action(const std::string &name, bf_rt_id_t *action_id) {
    auto bf_status = table->actionIdGet(name, action_id);

    if (bf_status != BF_SUCCESS) {
      std::cerr << "Error configuring table action " << name << "\n";
      exit(1);
    }
  }

  void init_actions(std::unordered_map<std::string, bf_rt_id_t *> actions) {
    for (const auto &action : actions) {
      init_action(action.first, action.second);
    }
  }

  size_t get_size() const {
    size_t size;
    auto bf_status = table->tableSizeGet(*session, dev_tgt, &size);
    assert(bf_status == BF_SUCCESS);
    return size;
  }

  size_t get_usage() const {
    uint32_t usage;
    auto bf_status = table->tableUsageGet(
        *session, dev_tgt, bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW,
        &usage);
    assert(bf_status == BF_SUCCESS);
    return usage;
  }

public:
  void dump_data_fields();
  void dump_data_fields(std::ostream &);

  void dump();
  void dump(std::ostream &);

  static void dump_table_names(const bfrt::BfRtInfo *bfrtInfo);
};

class Port_HDL_Info : __Table {
private:
  // Key fields IDs
  bf_rt_id_t CONN_ID;
  bf_rt_id_t CHNL_ID;

  // Data field ids
  bf_rt_id_t DEV_PORT;

public:
  Port_HDL_Info() : __Table("$PORT_HDL_INFO") {
    auto bf_status = table->keyFieldIdGet("$CONN_ID", &CONN_ID);
    assert(bf_status == BF_SUCCESS);
    bf_status = table->keyFieldIdGet("$CHNL_ID", &CHNL_ID);
    assert(bf_status == BF_SUCCESS);

    bf_status = table->dataFieldIdGet("$DEV_PORT", &DEV_PORT);
    assert(bf_status == BF_SUCCESS);
  }

  uint16_t get_dev_port(uint16_t front_panel_port, uint16_t lane,
                        bool from_hw = false) {
    auto hwflag = from_hw ? bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW
                          : bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW;

    key_setup(front_panel_port, lane);
    auto bf_status =
        table->tableEntryGet(*session, dev_tgt, *key, hwflag, data.get());
    assert(bf_status == BF_SUCCESS);

    uint64_t value;
    bf_status = data->getValue(DEV_PORT, &value);
    assert(bf_status == BF_SUCCESS);

    return (uint16_t)value;
  }

private:
  void key_setup(uint16_t front_panel_port, uint16_t lane) {
    table->keyReset(key.get());

    auto bf_status =
        key->setValue(CONN_ID, static_cast<uint64_t>(front_panel_port));
    assert(bf_status == BF_SUCCESS);

    bf_status = key->setValue(CHNL_ID, static_cast<uint64_t>(lane));
    assert(bf_status == BF_SUCCESS);
  }
};

class Port_Stat : __Table {
private:
  struct key_fields_t {
    // Key fields IDs
    bf_rt_id_t dev_port;
  };

  struct data_fields_t {
    // Data field ids
    bf_rt_id_t FramesReceivedOK;
    bf_rt_id_t FramesReceivedAll;
    bf_rt_id_t FramesReceivedwithFCSError;
    bf_rt_id_t FrameswithanyError;
    bf_rt_id_t OctetsReceivedinGoodFrames;
    bf_rt_id_t OctetsReceived;
    bf_rt_id_t FramesReceivedwithUnicastAddresses;
    bf_rt_id_t FramesReceivedwithMulticastAddresses;
    bf_rt_id_t FramesReceivedwithBroadcastAddresses;
    bf_rt_id_t FramesReceivedoftypePAUSE;
    bf_rt_id_t FramesReceivedwithLengthError;
    bf_rt_id_t FramesReceivedUndersized;
    bf_rt_id_t FramesReceivedOversized;
    bf_rt_id_t FragmentsReceived;
    bf_rt_id_t JabberReceived;
    bf_rt_id_t PriorityPauseFrames;
    bf_rt_id_t CRCErrorStomped;
    bf_rt_id_t FrameTooLong;
    bf_rt_id_t RxVLANFramesGood;
    bf_rt_id_t FramesDroppedBufferFull;
    bf_rt_id_t FramesReceivedLength_lt_64;
    bf_rt_id_t FramesReceivedLength_eq_64;
    bf_rt_id_t FramesReceivedLength_65_127;
    bf_rt_id_t FramesReceivedLength_128_255;
    bf_rt_id_t FramesReceivedLength_256_511;
    bf_rt_id_t FramesReceivedLength_512_1023;
    bf_rt_id_t FramesReceivedLength_1024_1518;
    bf_rt_id_t FramesReceivedLength_1519_2047;
    bf_rt_id_t FramesReceivedLength_2048_4095;
    bf_rt_id_t FramesReceivedLength_4096_8191;
    bf_rt_id_t FramesReceivedLength_8192_9215;
    bf_rt_id_t FramesReceivedLength_9216;
    bf_rt_id_t FramesTransmittedOK;
    bf_rt_id_t FramesTransmittedAll;
    bf_rt_id_t FramesTransmittedwithError;
    bf_rt_id_t OctetsTransmittedwithouterror;
    bf_rt_id_t OctetsTransmittedTotal;
    bf_rt_id_t FramesTransmittedUnicast;
    bf_rt_id_t FramesTransmittedMulticast;
    bf_rt_id_t FramesTransmittedBroadcast;
    bf_rt_id_t FramesTransmittedPause;
    bf_rt_id_t FramesTransmittedPriPause;
    bf_rt_id_t FramesTransmittedVLAN;
    bf_rt_id_t FramesTransmittedLength_lt_64;
    bf_rt_id_t FramesTransmittedLength_eq_64;
    bf_rt_id_t FramesTransmittedLength_65_127;
    bf_rt_id_t FramesTransmittedLength_128_255;
    bf_rt_id_t FramesTransmittedLength_256_511;
    bf_rt_id_t FramesTransmittedLength_512_1023;
    bf_rt_id_t FramesTransmittedLength_1024_1518;
    bf_rt_id_t FramesTransmittedLength_1519_2047;
    bf_rt_id_t FramesTransmittedLength_2048_4095;
    bf_rt_id_t FramesTransmittedLength_4096_8191;
    bf_rt_id_t FramesTransmittedLength_8192_9215;
    bf_rt_id_t FramesTransmittedLength_9216;
    bf_rt_id_t Pri0FramesTransmitted;
    bf_rt_id_t Pri1FramesTransmitted;
    bf_rt_id_t Pri2FramesTransmitted;
    bf_rt_id_t Pri3FramesTransmitted;
    bf_rt_id_t Pri4FramesTransmitted;
    bf_rt_id_t Pri5FramesTransmitted;
    bf_rt_id_t Pri6FramesTransmitted;
    bf_rt_id_t Pri7FramesTransmitted;
    bf_rt_id_t Pri0FramesReceived;
    bf_rt_id_t Pri1FramesReceived;
    bf_rt_id_t Pri2FramesReceived;
    bf_rt_id_t Pri3FramesReceived;
    bf_rt_id_t Pri4FramesReceived;
    bf_rt_id_t Pri5FramesReceived;
    bf_rt_id_t Pri6FramesReceived;
    bf_rt_id_t Pri7FramesReceived;
    bf_rt_id_t TransmitPri0Pause1USCount;
    bf_rt_id_t TransmitPri1Pause1USCount;
    bf_rt_id_t TransmitPri2Pause1USCount;
    bf_rt_id_t TransmitPri3Pause1USCount;
    bf_rt_id_t TransmitPri4Pause1USCount;
    bf_rt_id_t TransmitPri5Pause1USCount;
    bf_rt_id_t TransmitPri6Pause1USCount;
    bf_rt_id_t TransmitPri7Pause1USCount;
    bf_rt_id_t ReceivePri0Pause1USCount;
    bf_rt_id_t ReceivePri1Pause1USCount;
    bf_rt_id_t ReceivePri2Pause1USCount;
    bf_rt_id_t ReceivePri3Pause1USCount;
    bf_rt_id_t ReceivePri4Pause1USCount;
    bf_rt_id_t ReceivePri5Pause1USCount;
    bf_rt_id_t ReceivePri6Pause1USCount;
    bf_rt_id_t ReceivePri7Pause1USCount;
    bf_rt_id_t ReceiveStandardPause1USCount;
    bf_rt_id_t FramesTruncated;
  };

  key_fields_t key_fields;
  data_fields_t data_fields;

public:
  Port_Stat() : __Table("$PORT_STAT") {
    init_key({
        {"$DEV_PORT", &key_fields.dev_port},
    });

    init_data({
        {"$FramesReceivedOK", &data_fields.FramesReceivedOK},
        {"$FramesReceivedAll", &data_fields.FramesReceivedAll},
        {"$FramesReceivedwithFCSError",
         &data_fields.FramesReceivedwithFCSError},
        {"$FrameswithanyError", &data_fields.FrameswithanyError},
        {"$OctetsReceivedinGoodFrames",
         &data_fields.OctetsReceivedinGoodFrames},
        {"$OctetsReceived", &data_fields.OctetsReceived},
        {"$FramesReceivedwithUnicastAddresses",
         &data_fields.FramesReceivedwithUnicastAddresses},
        {"$FramesReceivedwithMulticastAddresses",
         &data_fields.FramesReceivedwithMulticastAddresses},
        {"$FramesReceivedwithBroadcastAddresses",
         &data_fields.FramesReceivedwithBroadcastAddresses},
        {"$FramesReceivedoftypePAUSE", &data_fields.FramesReceivedoftypePAUSE},
        {"$FramesReceivedwithLengthError",
         &data_fields.FramesReceivedwithLengthError},
        {"$FramesReceivedUndersized", &data_fields.FramesReceivedUndersized},
        {"$FramesReceivedOversized", &data_fields.FramesReceivedOversized},
        {"$FragmentsReceived", &data_fields.FragmentsReceived},
        {"$JabberReceived", &data_fields.JabberReceived},
        {"$PriorityPauseFrames", &data_fields.PriorityPauseFrames},
        {"$CRCErrorStomped", &data_fields.CRCErrorStomped},
        {"$FrameTooLong", &data_fields.FrameTooLong},
        {"$RxVLANFramesGood", &data_fields.RxVLANFramesGood},
        {"$FramesDroppedBufferFull", &data_fields.FramesDroppedBufferFull},
        {"$FramesReceivedLength_lt_64",
         &data_fields.FramesReceivedLength_lt_64},
        {"$FramesReceivedLength_eq_64",
         &data_fields.FramesReceivedLength_eq_64},
        {"$FramesReceivedLength_65_127",
         &data_fields.FramesReceivedLength_65_127},
        {"$FramesReceivedLength_128_255",
         &data_fields.FramesReceivedLength_128_255},
        {"$FramesReceivedLength_256_511",
         &data_fields.FramesReceivedLength_256_511},
        {"$FramesReceivedLength_512_1023",
         &data_fields.FramesReceivedLength_512_1023},
        {"$FramesReceivedLength_1024_1518",
         &data_fields.FramesReceivedLength_1024_1518},
        {"$FramesReceivedLength_1519_2047",
         &data_fields.FramesReceivedLength_1519_2047},
        {"$FramesReceivedLength_2048_4095",
         &data_fields.FramesReceivedLength_2048_4095},
        {"$FramesReceivedLength_4096_8191",
         &data_fields.FramesReceivedLength_4096_8191},
        {"$FramesReceivedLength_8192_9215",
         &data_fields.FramesReceivedLength_8192_9215},
        {"$FramesReceivedLength_9216", &data_fields.FramesReceivedLength_9216},
        {"$FramesTransmittedOK", &data_fields.FramesTransmittedOK},
        {"$FramesTransmittedAll", &data_fields.FramesTransmittedAll},
        {"$FramesTransmittedwithError",
         &data_fields.FramesTransmittedwithError},
        {"$OctetsTransmittedwithouterror",
         &data_fields.OctetsTransmittedwithouterror},
        {"$OctetsTransmittedTotal", &data_fields.OctetsTransmittedTotal},
        {"$FramesTransmittedUnicast", &data_fields.FramesTransmittedUnicast},
        {"$FramesTransmittedMulticast",
         &data_fields.FramesTransmittedMulticast},
        {"$FramesTransmittedBroadcast",
         &data_fields.FramesTransmittedBroadcast},
        {"$FramesTransmittedPause", &data_fields.FramesTransmittedPause},
        {"$FramesTransmittedPriPause", &data_fields.FramesTransmittedPriPause},
        {"$FramesTransmittedVLAN", &data_fields.FramesTransmittedVLAN},
        {"$FramesTransmittedLength_lt_64",
         &data_fields.FramesTransmittedLength_lt_64},
        {"$FramesTransmittedLength_eq_64",
         &data_fields.FramesTransmittedLength_eq_64},
        {"$FramesTransmittedLength_65_127",
         &data_fields.FramesTransmittedLength_65_127},
        {"$FramesTransmittedLength_128_255",
         &data_fields.FramesTransmittedLength_128_255},
        {"$FramesTransmittedLength_256_511",
         &data_fields.FramesTransmittedLength_256_511},
        {"$FramesTransmittedLength_512_1023",
         &data_fields.FramesTransmittedLength_512_1023},
        {"$FramesTransmittedLength_1024_1518",
         &data_fields.FramesTransmittedLength_1024_1518},
        {"$FramesTransmittedLength_1519_2047",
         &data_fields.FramesTransmittedLength_1519_2047},
        {"$FramesTransmittedLength_2048_4095",
         &data_fields.FramesTransmittedLength_2048_4095},
        {"$FramesTransmittedLength_4096_8191",
         &data_fields.FramesTransmittedLength_4096_8191},
        {"$FramesTransmittedLength_8192_9215",
         &data_fields.FramesTransmittedLength_8192_9215},
        {"$FramesTransmittedLength_9216",
         &data_fields.FramesTransmittedLength_9216},
        {"$Pri0FramesTransmitted", &data_fields.Pri0FramesTransmitted},
        {"$Pri1FramesTransmitted", &data_fields.Pri1FramesTransmitted},
        {"$Pri2FramesTransmitted", &data_fields.Pri2FramesTransmitted},
        {"$Pri3FramesTransmitted", &data_fields.Pri3FramesTransmitted},
        {"$Pri4FramesTransmitted", &data_fields.Pri4FramesTransmitted},
        {"$Pri5FramesTransmitted", &data_fields.Pri5FramesTransmitted},
        {"$Pri6FramesTransmitted", &data_fields.Pri6FramesTransmitted},
        {"$Pri7FramesTransmitted", &data_fields.Pri7FramesTransmitted},
        {"$Pri0FramesReceived", &data_fields.Pri0FramesReceived},
        {"$Pri1FramesReceived", &data_fields.Pri1FramesReceived},
        {"$Pri2FramesReceived", &data_fields.Pri2FramesReceived},
        {"$Pri3FramesReceived", &data_fields.Pri3FramesReceived},
        {"$Pri4FramesReceived", &data_fields.Pri4FramesReceived},
        {"$Pri5FramesReceived", &data_fields.Pri5FramesReceived},
        {"$Pri6FramesReceived", &data_fields.Pri6FramesReceived},
        {"$Pri7FramesReceived", &data_fields.Pri7FramesReceived},
        {"$TransmitPri0Pause1USCount", &data_fields.TransmitPri0Pause1USCount},
        {"$TransmitPri1Pause1USCount", &data_fields.TransmitPri1Pause1USCount},
        {"$TransmitPri2Pause1USCount", &data_fields.TransmitPri2Pause1USCount},
        {"$TransmitPri3Pause1USCount", &data_fields.TransmitPri3Pause1USCount},
        {"$TransmitPri4Pause1USCount", &data_fields.TransmitPri4Pause1USCount},
        {"$TransmitPri5Pause1USCount", &data_fields.TransmitPri5Pause1USCount},
        {"$TransmitPri6Pause1USCount", &data_fields.TransmitPri6Pause1USCount},
        {"$TransmitPri7Pause1USCount", &data_fields.TransmitPri7Pause1USCount},
        {"$ReceivePri0Pause1USCount", &data_fields.ReceivePri0Pause1USCount},
        {"$ReceivePri1Pause1USCount", &data_fields.ReceivePri1Pause1USCount},
        {"$ReceivePri2Pause1USCount", &data_fields.ReceivePri2Pause1USCount},
        {"$ReceivePri3Pause1USCount", &data_fields.ReceivePri3Pause1USCount},
        {"$ReceivePri4Pause1USCount", &data_fields.ReceivePri4Pause1USCount},
        {"$ReceivePri5Pause1USCount", &data_fields.ReceivePri5Pause1USCount},
        {"$ReceivePri6Pause1USCount", &data_fields.ReceivePri6Pause1USCount},
        {"$ReceivePri7Pause1USCount", &data_fields.ReceivePri7Pause1USCount},
        {"$ReceiveStandardPause1USCount",
         &data_fields.ReceiveStandardPause1USCount},
        {"$FramesTruncated", &data_fields.FramesTruncated},
    });
  }

  uint64_t get_port_rx(uint16_t dev_port, bool from_hw = false) {
    auto hwflag = from_hw ? bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW
                          : bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW;

    key_setup(dev_port);

    auto bf_status =
        table->tableEntryGet(*session, dev_tgt, *key, hwflag, data.get());
    assert(bf_status == BF_SUCCESS);

    uint64_t value;
    bf_status = data->getValue(data_fields.FramesReceivedOK, &value);
    assert(bf_status == BF_SUCCESS);

    return value;
  }

  uint64_t get_port_tx(uint16_t dev_port, bool from_hw = false) {
    auto hwflag = from_hw ? bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW
                          : bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW;

    key_setup(dev_port);

    auto bf_status =
        table->tableEntryGet(*session, dev_tgt, *key, hwflag, data.get());
    assert(bf_status == BF_SUCCESS);

    uint64_t value;
    bf_status = data->getValue(data_fields.FramesTransmittedOK, &value);
    assert(bf_status == BF_SUCCESS);

    return value;
  }

private:
  void key_setup(uint16_t dev_port) {
    table->keyReset(key.get());
    assert(key);

    auto bf_status =
        key->setValue(key_fields.dev_port, static_cast<uint64_t>(dev_port));
    assert(bf_status == BF_SUCCESS);
  }
};

class Ports : __Table {
private:
  // Key fields IDs
  bf_rt_id_t DEV_PORT;

  // Data field ids
  bf_rt_id_t SPEED;
  bf_rt_id_t FEC;
  bf_rt_id_t PORT_ENABLE;
  bf_rt_id_t LOOPBACK_MODE;
  bf_rt_id_t PORT_UP;

  Port_HDL_Info port_hdl_info;
  Port_Stat port_stat;

public:
  Ports() : __Table("$PORT"), port_hdl_info(), port_stat() {
    auto bf_status = table->keyFieldIdGet("$DEV_PORT", &DEV_PORT);
    assert(bf_status == BF_SUCCESS);

    bf_status = table->dataFieldIdGet("$SPEED", &SPEED);
    assert(bf_status == BF_SUCCESS);

    bf_status = table->dataFieldIdGet("$FEC", &FEC);
    assert(bf_status == BF_SUCCESS);

    bf_status = table->dataFieldIdGet("$PORT_ENABLE", &PORT_ENABLE);
    assert(bf_status == BF_SUCCESS);

    bf_status = table->dataFieldIdGet("$LOOPBACK_MODE", &LOOPBACK_MODE);
    assert(bf_status == BF_SUCCESS);

    bf_status = table->dataFieldIdGet("$PORT_UP", &PORT_UP);
    assert(bf_status == BF_SUCCESS);
  }

  void add_dev_port(uint16_t dev_port, bf_port_speed_t speed,
                    bf_loopback_mode_e loopback_mode = BF_LPBK_NONE) {
    std::map<bf_port_speed_t, std::string> speed_opts{
        {BF_SPEED_NONE, "BF_SPEED_10G"},  {BF_SPEED_25G, "BF_SPEED_25G"},
        {BF_SPEED_40G, "BF_SPEED_40G"},   {BF_SPEED_50G, "BF_SPEED_50G"},
        {BF_SPEED_100G, "BF_SPEED_100G"},
    };

    std::map<bf_fec_type_t, std::string> fec_opts{
        {BF_FEC_TYP_NONE, "BF_FEC_TYP_NONE"},
        {BF_FEC_TYP_FIRECODE, "BF_FEC_TYP_FIRECODE"},
        {BF_FEC_TYP_REED_SOLOMON, "BF_FEC_TYP_REED_SOLOMON"},
    };

    std::map<bf_port_speed_t, bf_fec_type_t> speed_to_fec{
        {BF_SPEED_NONE, BF_FEC_TYP_NONE},
        {BF_SPEED_25G, BF_FEC_TYP_NONE},
        {BF_SPEED_40G, BF_FEC_TYP_NONE},
        {BF_SPEED_50G, BF_FEC_TYP_NONE},
        {BF_SPEED_50G, BF_FEC_TYP_NONE},
        {BF_SPEED_100G, BF_FEC_TYP_REED_SOLOMON},
    };

    std::map<bf_loopback_mode_e, std::string> loopback_mode_opts{
        {BF_LPBK_NONE, "BF_LPBK_NONE"},
        {BF_LPBK_MAC_NEAR, "BF_LPBK_MAC_NEAR"},
        {BF_LPBK_MAC_FAR, "BF_LPBK_MAC_FAR"},
        {BF_LPBK_PCS_NEAR, "BF_LPBK_PCS_NEAR"},
        {BF_LPBK_SERDES_NEAR, "BF_LPBK_SERDES_NEAR"},
        {BF_LPBK_SERDES_FAR, "BF_LPBK_SERDES_FAR"},
        {BF_LPBK_PIPE, "BF_LPBK_PIPE"},
    };

    auto fec = speed_to_fec[speed];

    key_setup(dev_port);
    data_setup(speed_opts[speed], fec_opts[fec], true,
               loopback_mode_opts[loopback_mode]);

    auto bf_status = table->tableEntryAdd(*session, dev_tgt, *key, *data);
    assert(bf_status == BF_SUCCESS);
  }

  void add_port(uint16_t front_panel_port, uint16_t lane,
                bf_port_speed_t speed) {
    auto dev_port = port_hdl_info.get_dev_port(front_panel_port, lane, false);
    add_dev_port(dev_port, speed);
  }

  bool is_port_up(uint16_t dev_port, bool from_hw = false) {
    auto hwflag = from_hw ? bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW
                          : bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW;

    key_setup(dev_port);

    auto bf_status =
        table->tableEntryGet(*session, dev_tgt, *key, hwflag, data.get());
    assert(bf_status == BF_SUCCESS);

    bool value;
    bf_status = data->getValue(PORT_UP, &value);
    assert(bf_status == BF_SUCCESS);

    return value;
  }

  uint16_t get_dev_port(uint16_t front_panel_port, uint16_t lane) {
    return port_hdl_info.get_dev_port(front_panel_port, lane, false);
  }

  uint64_t get_port_rx(uint16_t port) { return port_stat.get_port_rx(port); }
  uint64_t get_port_tx(uint16_t port) { return port_stat.get_port_tx(port); }

private:
  void key_setup(uint16_t dev_port) {
    table->keyReset(key.get());

    auto bf_status = key->setValue(DEV_PORT, static_cast<uint64_t>(dev_port));
    assert(bf_status == BF_SUCCESS);
  }

  void data_setup(std::string speed, std::string fec, bool port_enable,
                  std::string loopback_mode) {
    table->dataReset(data.get());

    auto bf_status = data->setValue(SPEED, speed);
    assert(bf_status == BF_SUCCESS);

    bf_status = data->setValue(FEC, fec);
    assert(bf_status == BF_SUCCESS);

    bf_status = data->setValue(PORT_ENABLE, port_enable);
    assert(bf_status == BF_SUCCESS);

    bf_status = data->setValue(LOOPBACK_MODE, loopback_mode);
    assert(bf_status == BF_SUCCESS);
  }

public:
  static bf_port_speed_t gbps_to_bf_port_speed(uint32_t gbps) {
    switch (gbps) {
    case 100:
      return BF_SPEED_100G;
    case 50:
      return BF_SPEED_50G;
    case 40:
      return BF_SPEED_40G;
    case 25:
      return BF_SPEED_25G;
    default:
      return BF_SPEED_NONE;
    }
  }
};

typedef uint64_t field_value_t;

struct fields_values_t {
  uint32_t size;
  field_value_t *values;

  fields_values_t(uint32_t _size)
      : size(_size), values(new field_value_t[_size]) {}

  fields_values_t(const fields_values_t &key)
      : size(key.size), values(new field_value_t[key.size]) {
    for (auto i = 0u; i < size; i++) {
      values[i] = key.values[i];
    }
  }

  ~fields_values_t() { delete values; }

  struct hash {
    std::size_t operator()(const fields_values_t &k) const {
      int hash = 0;

      for (auto i = 0u; i < k.size; i++) {
        hash ^= std::hash<int>()(k.values[i]);
      }

      return hash;
    }
  };
};

inline bool operator==(const fields_values_t &lhs, const fields_values_t &rhs) {
  if (lhs.size != rhs.size) {
    return false;
  }

  for (auto i = 0u; i < lhs.size; i++) {
    if (lhs.values[i] != rhs.values[i]) {
      return false;
    }
  }

  return true;
}

typedef uint8_t byte_t;

struct bytes_t {
  byte_t *values;
  uint32_t size;

  bytes_t() : values(nullptr), size(0) {}

  bytes_t(uint32_t _size) : values(new byte_t[_size]), size(_size) {}

  bytes_t(const bytes_t &key) : bytes_t(key.size) {
    for (auto i = 0u; i < size; i++) {
      values[i] = key.values[i];
    }
  }

  ~bytes_t() {
    if (values) {
      delete[] values;
      values = nullptr;
      size = 0;
    }
  }

  byte_t &operator[](int i) {
    assert(i < size);
    return values[i];
  }

  const byte_t &operator[](int i) const {
    assert(i < size);
    return values[i];
  }

  // copy assignment
  bytes_t &operator=(const bytes_t &other) noexcept {
    // Guard self assignment
    if (this == &other) {
      return *this;
    }

    if (size != other.size) {
      this->~bytes_t();
      values = new byte_t[other.size];
      size = other.size;
    }

    std::copy(other.values, other.values + other.size, values);

    return *this;
  }

  struct hash {
    std::size_t operator()(const bytes_t &k) const {
      int hash = 0;

      for (auto i = 0u; i < k.size; i++) {
        hash ^= std::hash<int>()(k.values[i]);
      }

      return hash;
    }
  };
};

inline bool operator==(const bytes_t &lhs, const bytes_t &rhs) {
  if (lhs.size != rhs.size) {
    return false;
  }

  for (auto i = 0u; i < lhs.size; i++) {
    if (lhs.values[i] != rhs.values[i]) {
      return false;
    }
  }

  return true;
}

inline std::ostream &operator<<(std::ostream &os, const bytes_t &bytes) {
  os << "{";
  for (int i = 0u; i < bytes.size; i++) {
    if (i > 0)
      os << ",";
    os << (int)bytes[i];
  }
  os << "}";
  return os;
}

class Table : __Table {
private:
  bf_rt_id_t populate_action_id;
  std::unordered_set<fields_values_t, fields_values_t::hash> set_keys;

public:
  Table(const std::string _table_name) : __Table("Ingress." + _table_name) {
    init_key();
    init_action("Ingress.populate", &populate_action_id);
    init_data_with_action(populate_action_id);
  }

  void set(const bytes_t &key_bytes, const bytes_t &param_bytes) {
    auto keys = bytes_to_fields_values(key_bytes, key_fields);
    auto params = bytes_to_fields_values(param_bytes, data_fields);

    if (set_keys.find(keys) == set_keys.end()) {
      add(keys, params);
    } else {
      mod(keys, params);
    }
  }

  static std::unique_ptr<Table> build(const std::string _table_name) {
    return std::unique_ptr<Table>(new Table(_table_name));
  }

private:
  void key_setup(const fields_values_t &key_field_values) {
    auto bf_status = table->keyReset(key.get());
    assert(bf_status == BF_SUCCESS);

    for (auto i = 0u; i < key_fields.size(); i++) {
      auto key_field_value = key_field_values.values[i];
      auto key_field = key_fields[i];
      auto bf_status = key->setValue(key_field.id, key_field_value);
      assert(bf_status == BF_SUCCESS);
    }
  }

  void data_setup(const fields_values_t &param_field_values) {
    auto bf_status = table->dataReset(populate_action_id, data.get());
    assert(bf_status == BF_SUCCESS);

    for (auto i = 0u; i < data_fields.size(); i++) {
      auto param_field_value = param_field_values.values[i];
      auto param_field = data_fields[i];
      auto bf_status = data->setValue(param_field.id, param_field_value);
      assert(bf_status == BF_SUCCESS);
    }
  }

  void add(const fields_values_t &key_fields_values,
           const fields_values_t &param_fields_values) {
    key_setup(key_fields_values);
    data_setup(param_fields_values);

    auto bf_status = table->tableEntryAdd(*session, dev_tgt, *key, *data);
    assert(bf_status == BF_SUCCESS);

    set_keys.insert(key_fields_values);
  }

  void mod(const fields_values_t &key_fields_values,
           const fields_values_t &param_fields_values) {
    key_setup(key_fields_values);
    data_setup(param_fields_values);

    auto bf_status = table->tableEntryMod(*session, dev_tgt, *key, *data);
    assert(bf_status == BF_SUCCESS);
  }

  void del(const fields_values_t &key_fields_values,
           const fields_values_t &param_fields_values) {
    key_setup(key_fields_values);
    data_setup(param_fields_values);

    auto bf_status = table->tableEntryDel(*session, dev_tgt, 0, *key);
    assert(bf_status == BF_SUCCESS);

    set_keys.erase(key_fields_values);
  }

  static fields_values_t
  bytes_to_fields_values(const bytes_t &values,
                         const std::vector<field_t> &fields) {
    auto fields_values = fields_values_t(fields.size());
    auto starting_byte = 0;

    for (auto kf_i = 0u; kf_i < fields.size(); kf_i++) {
      auto kf = fields[kf_i];
      auto bytes = kf.size / 8;

      assert(values.size >= starting_byte + bytes);

      fields_values.values[kf_i] = 0;

      for (auto i = 0u; i < bytes; i++) {
        auto byte = values[starting_byte + i];

        fields_values.values[kf_i] = (fields_values.values[kf_i] << 8) | byte;
      }

      starting_byte += bytes;
    }

    return fields_values;
  }
};

template <typename T>
class Map {
private:
  std::unordered_map<bytes_t, T, bytes_t::hash> data;
  std::vector<std::unique_ptr<Table>> dataplane_tables;

public:
  Map() {}

  Map(const std::vector<std::string> &dataplane_table_names) {
    for (const auto &table_name : dataplane_table_names) {
      dataplane_tables.push_back(Table::build(table_name));
    }
  }

  bool get(bytes_t key, T &value) const {
    if (contains(key)) {
      value = data.at(key);
      return true;
    }

    return false;
  }

  bool contains(bytes_t key) const { return data.find(key) != data.end(); }

  void put(bytes_t key, T value) {
    data[key] = value;

    auto value_bytes = to_bytes(value);
    for (auto &dataplane_table : dataplane_tables) {
      dataplane_table->set(key, value_bytes);
    }
  }

  static std::unique_ptr<Map> build() {
    return std::unique_ptr<Map>(new Map());
  }

  static std::unique_ptr<Map> build(
    const std::vector<std::string> &dataplane_table_names) {
    return std::unique_ptr<Map>(new Map(dataplane_table_names));
  }

private:
  bytes_t to_bytes(int value) {
    bytes_t bytes(4);

    bytes[0] = (value >> 24) & 0xff;
    bytes[1] = (value >> 16) & 0xff;
    bytes[2] = (value >> 8) & 0xff;
    bytes[3] = (value >> 0) & 0xff;

    return bytes;
  }

  bytes_t to_bytes(bytes_t value) { return value; }
};

#define DCHAIN_RESERVED (2)

class Dchain {
public:
  Dchain(int index_range) {
    cells = (struct dchain_cell *)malloc(sizeof(struct dchain_cell) *
                                         (index_range + DCHAIN_RESERVED));
    timestamps = (time_ns_t *)malloc(sizeof(time_ns_t) * (index_range));

    impl_init(index_range);
  }

  uint32_t allocate_new_index(uint32_t &index_out, time_ns_t time) {
    uint32_t ret = impl_allocate_new_index(index_out);

    if (ret) {
      timestamps[index_out] = time;
    }

    return ret;
  }

  uint32_t rejuvenate_index(uint32_t index, time_ns_t time) {
    uint32_t ret = impl_rejuvenate_index(index);

    if (ret) {
      timestamps[index] = time;
    }

    return ret;
  }

  uint32_t expire_one_index(uint32_t &index_out, time_ns_t time) {
    uint32_t has_ind = impl_get_oldest_index(index_out);

    if (has_ind) {
      if (timestamps[index_out] < time) {
        uint32_t rez = impl_free_index(index_out);
        return rez;
      }
    }

    return 0;
  }

  uint32_t is_index_allocated(uint32_t index) {
    return impl_is_index_allocated(index);
  }

  uint32_t free_index(uint32_t index) { return impl_free_index(index); }

  ~Dchain() {
    free(cells);
    free(timestamps);
  }

  static std::unique_ptr<Dchain> build(int index_range) {
    return std::unique_ptr<Dchain>(new Dchain(index_range));
  }

private:
  struct dchain_cell {
    uint32_t prev;
    uint32_t next;
  };

  enum DCHAIN_ENUM {
    ALLOC_LIST_HEAD = 0,
    FREE_LIST_HEAD = 1,
    INDEX_SHIFT = DCHAIN_RESERVED
  };

  struct dchain_cell *cells;
  time_ns_t *timestamps;

  void impl_init(uint32_t size) {
    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;
    al_head->prev = 0;
    al_head->next = 0;
    uint32_t i = INDEX_SHIFT;

    struct dchain_cell *fl_head = cells + FREE_LIST_HEAD;
    fl_head->next = i;
    fl_head->prev = fl_head->next;

    while (i < (size + INDEX_SHIFT - 1)) {
      struct dchain_cell *current = cells + i;
      current->next = i + 1;
      current->prev = current->next;

      ++i;
    }

    struct dchain_cell *last = cells + i;
    last->next = FREE_LIST_HEAD;
    last->prev = last->next;
  }

  uint32_t impl_allocate_new_index(uint32_t &index) {
    struct dchain_cell *fl_head = cells + FREE_LIST_HEAD;
    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;
    uint32_t allocated = fl_head->next;
    if (allocated == FREE_LIST_HEAD) {
      return 0;
    }

    struct dchain_cell *allocp = cells + allocated;
    // Extract the link from the "empty" chain.
    fl_head->next = allocp->next;
    fl_head->prev = fl_head->next;

    // Add the link to the "new"-end "alloc" chain.
    allocp->next = ALLOC_LIST_HEAD;
    allocp->prev = al_head->prev;

    struct dchain_cell *alloc_head_prevp = cells + al_head->prev;
    alloc_head_prevp->next = allocated;
    al_head->prev = allocated;

    index = allocated - INDEX_SHIFT;

    return 1;
  }

  uint32_t impl_free_index(uint32_t index) {
    uint32_t freed = index + INDEX_SHIFT;

    struct dchain_cell *freedp = cells + freed;
    uint32_t freed_prev = freedp->prev;
    uint32_t freed_next = freedp->next;

    // The index is already free.
    if (freed_next == freed_prev) {
      if (freed_prev != ALLOC_LIST_HEAD) {
        return 0;
      }
    }

    struct dchain_cell *fr_head = cells + FREE_LIST_HEAD;
    struct dchain_cell *freed_prevp = cells + freed_prev;
    freed_prevp->next = freed_next;

    struct dchain_cell *freed_nextp = cells + freed_next;
    freed_nextp->prev = freed_prev;

    freedp->next = fr_head->next;
    freedp->prev = freedp->next;

    fr_head->next = freed;
    fr_head->prev = fr_head->next;

    return 1;
  }

  uint32_t impl_get_oldest_index(uint32_t &index) {
    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;

    // No allocated indexes.
    if (al_head->next == ALLOC_LIST_HEAD) {
      return 0;
    }

    index = al_head->next - INDEX_SHIFT;

    return 1;
  }

  uint32_t impl_rejuvenate_index(uint32_t index) {
    uint32_t lifted = index + INDEX_SHIFT;

    struct dchain_cell *liftedp = cells + lifted;
    uint32_t lifted_next = liftedp->next;
    uint32_t lifted_prev = liftedp->prev;

    if (lifted_next == lifted_prev) {
      if (lifted_next != ALLOC_LIST_HEAD) {
        return 0;
      } else {
        return 1;
      }
    }

    struct dchain_cell *lifted_prevp = cells + lifted_prev;
    lifted_prevp->next = lifted_next;

    struct dchain_cell *lifted_nextp = cells + lifted_next;
    lifted_nextp->prev = lifted_prev;

    struct dchain_cell *al_head = cells + ALLOC_LIST_HEAD;
    uint32_t al_head_prev = al_head->prev;

    liftedp->next = ALLOC_LIST_HEAD;
    liftedp->prev = al_head_prev;

    struct dchain_cell *al_head_prevp = cells + al_head_prev;
    al_head_prevp->next = lifted;

    al_head->prev = lifted;
    return 1;
  }

  uint32_t impl_is_index_allocated(uint32_t index) {
    uint32_t lifted = index + INDEX_SHIFT;

    struct dchain_cell *liftedp = cells + lifted;
    uint32_t lifted_next = liftedp->next;
    uint32_t lifted_prev = liftedp->prev;

    uint32_t result;
    if (lifted_next == lifted_prev) {
      if (lifted_next != ALLOC_LIST_HEAD) {
        return 0;
      } else {
        return 1;
      }
    } else {
      return 1;
    }
  }
};

void state_init();

int main(int argc, char **argv) {
  init_bf_switchd();
  setup_bf_session();
  state_init();
  register_pcie_pkt_ops();

  std::cerr << "\nController is ready.\n";

  // main thread sleeps
  while (1) {
    sleep(60);
  }

  return 0;
}

struct state_t {
  /*@{NF STATE DECL}@*/

  state_t() {
    /*@{NF STATE INIT}@*/
  }
};

std::unique_ptr<state_t> state;

void state_init() { state = std::unique_ptr<state_t>(new state_t()); }

bool nf_process(time_ns_t now, pkt_t &pkt) {
  /*@{NF PROCESS}@*/
}