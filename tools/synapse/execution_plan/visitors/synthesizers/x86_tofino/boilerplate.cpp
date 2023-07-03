#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <pthread.h>
#include <time.h>
#include <algorithm>

#include <bf_rt/bf_rt.hpp>

#ifdef __cplusplus
extern "C" {
#endif
#include <bf_rt/bf_rt_common.h>
#include <bf_switchd/bf_switchd.h>
#include <bfutils/bf_utils.h>  // required for bfshell
#include <port_mgr/bf_port_if.h>
#include <pkt_mgr/pkt_mgr_intf.h>
#ifdef __cplusplus
}
#endif

#define ALL_PIPES 0xffff

#define ENV_VAR_SDE_INSTALL "SDE_INSTALL"

#define SWITCH_PACKET_MAX_BUFFER_SIZE 10000
#define MTU 1500

#define SWAP_ENDIAN_16(v) \
	{ (v) = __bswap_16((v)); }
#define SWAP_ENDIAN_32(v) \
	{ (v) = __bswap_32((v)); }

#define LOG_BF_STATUS(status, fmt, ...)                               \
	if ((status) != BF_SUCCESS) {                                     \
		fprintf(stderr, "%d: [WARN] " fmt "\nStatus: %s\n", __LINE__, \
				##__VA_ARGS__, bf_err_str(status));                   \
		fflush(stderr);                                               \
	}

#define ASSERT_BF_STATUS(status, fmt, ...)                             \
	if ((status) != BF_SUCCESS) {                                      \
		fprintf(stderr, "%d: [ERROR] " fmt "\nStatus: %s\n", __LINE__, \
				##__VA_ARGS__, bf_err_str(status));                    \
		fflush(stderr);                                                \
		exit(1);                                                       \
	}

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...)                             \
	{                                                   \
		fprintf(stderr, "[DEBUG] " fmt, ##__VA_ARGS__); \
		fflush(stderr);                                 \
	}
#else
#define LOG_DEBUG(fmt, ...)
#endif

#define LOG(fmt, ...)                        \
	{                                        \
		fprintf(stderr, fmt, ##__VA_ARGS__); \
		fflush(stderr);                      \
	}

#define PROGRAM "timeouts"

#define WAIT_FOR_ENTER(msg)                                                 \
	{                                                                       \
		std::cout << msg;                                                   \
		fflush(stdout);                                                     \
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); \
	}

typedef size_t bits_t;

bf_rt_target_t dev_tgt;
const bfrt::BfRtInfo *info;
std::shared_ptr<bfrt::BfRtSession> session;
pthread_mutex_t transaction_mutex;
bf_switchd_context_t *switchd_ctx;

void setup_transaction() {
	if (pthread_mutex_init(&transaction_mutex, NULL) != 0) {
		LOG("pthread_mutex_init failed\n");
		exit(1);
	}
}

void begin_transaction() {
	pthread_mutex_lock(&transaction_mutex);

	bool atomic = true;
	auto bf_status = session->beginTransaction(atomic);
	ASSERT_BF_STATUS(bf_status, "Failed to start transaction");
}

void end_transaction() {
	bool hw_sync = true;
	auto bf_status = session->commitTransaction(hw_sync);
	ASSERT_BF_STATUS(bf_status, "Failed to commit transaction");

	pthread_mutex_unlock(&transaction_mutex);
}

void begin_batch() {
	pthread_mutex_lock(&transaction_mutex);

	auto bf_status = session->beginBatch();
	ASSERT_BF_STATUS(bf_status, "Failed to start batch");
}

void end_batch() {
	bool hw_sync = true;
	auto bf_status = session->endBatch(hw_sync);
	ASSERT_BF_STATUS(bf_status, "Failed to commit batch");

	pthread_mutex_unlock(&transaction_mutex);
}

struct cpu_t {
	uint16_t code_path;
	uint16_t in_port;
	uint16_t out_port;
	/*@{CPU HEADER DATAPLANE STATE FIELDS}@*/

	void dump() const {
		LOG("###[ CPU ]###\n");
		LOG("code path  %u\n", code_path);
		LOG("in port    %u\n", in_port);
		LOG("out port   %u\n", out_port);
	}
} __attribute__((packed));

struct eth_t {
	uint8_t dst_mac[6];
	uint8_t src_mac[6];
	uint16_t eth_type;

	void dump() const {
		LOG("###[ Ethernet ]###\n");
		LOG("dst  %02x:%02x:%02x:%02x:%02x:%02x\n", dst_mac[0], dst_mac[1],
			dst_mac[2], dst_mac[3], dst_mac[4], dst_mac[5]);
		LOG("src  %02x:%02x:%02x:%02x:%02x:%02x\n", src_mac[0], src_mac[1],
			src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
		LOG("type 0x%x\n", eth_type);
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
		LOG("###[ IP ]###\n");
		LOG("version %u\n", (version_ihl & 0xf0) >> 4);
		LOG("ihl     %u\n", (version_ihl & 0x0f));
		LOG("tos     %u\n", ecn_dscp);
		LOG("len     %u\n", ntohs(tot_len));
		LOG("id      %u\n", ntohs(id));
		LOG("off     %u\n", ntohs(frag_off));
		LOG("ttl     %u\n", ttl);
		LOG("proto   %u\n", protocol);
		LOG("chksum  0x%x\n", ntohs(check));
		LOG("src     %u.%u.%u.%u\n", (src_ip >> 0) & 0xff, (src_ip >> 8) & 0xff,
			(src_ip >> 16) & 0xff, (src_ip >> 24) & 0xff);
		LOG("dst     %u.%u.%u.%u\n", (dst_ip >> 0) & 0xff, (dst_ip >> 8) & 0xff,
			(dst_ip >> 16) & 0xff, (dst_ip >> 24) & 0xff);
	}
} __attribute__((packed));

struct tcpudp_t {
	uint16_t src_port;
	uint16_t dst_port;

	void dump() const {
		LOG("###[ TCP/UDP ]###\n");
		LOG("sport   %u\n", ntohs(src_port));
		LOG("dport   %u\n", ntohs(dst_port));
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
	if (l3_len < sizeof(ipv4_t)) return 0;

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
	if (cksum == 0 && ipv4_hdr->protocol == IPPROTO_UDP) cksum = 0xffff;

	return (uint16_t)cksum;
}

char *get_env_var_value(const char *env_var) {
	auto env_var_value = getenv(env_var);

	if (!env_var_value) {
		LOG("Env var \"%s\" not found\n", env_var);
		exit(1);
	}

	return env_var_value;
}

char *get_install_dir() { return get_env_var_value(ENV_VAR_SDE_INSTALL); }

typedef uint64_t time_ns_t;
typedef uint64_t time_ms_t;

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

	auto bf_status = bf_pkt_alloc(dev_tgt.dev_id, &tx_pkt, packet_size,
								  BF_DMA_CPU_PKT_TRANSMIT_0);

	ASSERT_BF_STATUS(bf_status, "Failed to allocate packet")

	bf_status = bf_pkt_data_copy(tx_pkt, pkt, packet_size);

	if (bf_status != BF_SUCCESS) {
		bf_pkt_free(device, tx_pkt);
		LOG_BF_STATUS(bf_status, "Failed to copy packet data (pkt_size=%d)",
					  packet_size)
		return;
	}

	bf_status = bf_pkt_tx(device, tx_pkt, BF_PKT_TX_RING_0, (void *)tx_pkt);

	if (bf_status != BF_SUCCESS) {
		LOG_BF_STATUS(bf_status, "Failed to transmit packet")
		bf_pkt_free(device, tx_pkt);
	}
}

bf_status_t txComplete(bf_dev_id_t device, bf_pkt_tx_ring_t tx_ring,
					   uint64_t tx_cookie, uint32_t status) {
	// Now we can free the packet.
	bf_pkt_free(device, (bf_pkt *)((uintptr_t)tx_cookie));
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
			LOG_DEBUG("Packet too large to Transmit - SKipping\n");
			break;
		}

		memcpy(bufp, pkt_buf, pkt_len);
		bufp += pkt_len;
		packet_size += pkt_len;
		pkt = bf_pkt_get_nextseg(pkt);
	} while (pkt);

	auto now = get_time();
	auto p = pkt_t((uint8_t *)in_packet, packet_size);

	begin_transaction();
	auto fwd = nf_process(now, p);
	end_transaction();

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
		auto bf_status = bf_pkt_rx_register(dev_tgt.dev_id, pcie_rx,
											(bf_pkt_rx_ring_t)rx_ring, 0);
		ASSERT_BF_STATUS(bf_status, "Failed to register PCIe callback")
	}
}

void init_bf_switchd(const std::string &program, bool run_in_background) {
	switchd_ctx =
		(bf_switchd_context_t *)calloc(1, sizeof(bf_switchd_context_t));

	/* Allocate memory to hold switchd configuration and state */
	if (switchd_ctx == NULL) {
		LOG("ERROR: Failed to allocate memory for switchd context\n");
		exit(1);
	}

	char target_conf_file[100];
	sprintf(target_conf_file, "%s/share/p4/targets/tofino/%s.conf",
			get_install_dir(), program.c_str());

	memset(switchd_ctx, 0, sizeof(bf_switchd_context_t));

	switchd_ctx->install_dir = get_install_dir();
	switchd_ctx->conf_file = const_cast<char *>(target_conf_file);
	switchd_ctx->skip_p4 = false;
	switchd_ctx->skip_port_add = false;
	switchd_ctx->running_in_background = run_in_background;
	switchd_ctx->dev_sts_thread = false;

	auto bf_status = bf_switchd_lib_init(switchd_ctx);

	ASSERT_BF_STATUS(bf_status, "Failed to initialize switchd")
}

void setup_bf_session(const std::string &program) {
	dev_tgt.dev_id = 0;
	dev_tgt.pipe_id = ALL_PIPES;

	// Get devMgr singleton instance
	auto &devMgr = bfrt::BfRtDevMgr::getInstance();

	// Get info object from dev_id and p4 program name
	auto bf_status = devMgr.bfRtInfoGet(dev_tgt.dev_id, program.c_str(), &info);
	ASSERT_BF_STATUS(bf_status, "Failed to get bfrt info")

	// Create a session object
	session = bfrt::BfRtSession::sessionCreate();
}

template <int key_size>
bool key_eq(void *k1, void *k2) {
	auto _k1 = (uint8_t *)k1;
	auto _k2 = (uint8_t *)k2;

	for (auto i = 0; i < key_size; i++) {
		if (_k1[i] != _k2[i]) {
			return false;
		}
	}

	return true;
}

template <int key_size>
unsigned key_hash(void *k) {
	auto _k = (uint8_t *)k;
	unsigned hash = 0;

	for (auto i = 0; i < key_size; i++) {
		hash = __builtin_ia32_crc32si(hash, _k[i]);
	}

	return hash;
}

class TofinoTable {
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
	TofinoTable(const std::string &_table_name)
		: table_name(_table_name), table(nullptr) {
		assert(info);
		assert(session);

		auto bf_status = info->bfrtTableFromNameGet(table_name, &table);
		ASSERT_BF_STATUS(bf_status, "Failed to get table from name (%s)",
						 table_name.c_str())

		// Allocate key and data once, and use reset across different uses
		bf_status = table->keyAllocate(&key);
		ASSERT_BF_STATUS(bf_status, "Failed to allocate key for table %s",
						 table_name.c_str())
		assert(key);

		bf_status = table->dataAllocate(&data);
		ASSERT_BF_STATUS(bf_status, "Failed to allocate data for table %s",
						 table_name.c_str())
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
			ASSERT_BF_STATUS(bf_status, "Failed to get key field name")

			bf_status = table->keyFieldSizeGet(id, &size);
			ASSERT_BF_STATUS(bf_status, "Failed to get key field size")

			key_fields.push_back({name, id, size});
		}

		LOG_DEBUG("Keys:\n");
		for (auto k : key_fields) {
			LOG_DEBUG("  %s (%lu bits)\n", k.name.c_str(), k.size);
		}
	}

	void init_key(const std::string &name, bf_rt_id_t *id) {
		auto bf_status = table->keyFieldIdGet(name, id);
		ASSERT_BF_STATUS(bf_status, "Failed to configure table key %s",
						 name.c_str())
	}

	void init_key(const std::unordered_map<std::string, bf_rt_id_t *> &fields) {
		for (const auto &field : fields) {
			init_key(field.first, field.second);
		}
	}

	void init_data(const std::string &name, bf_rt_id_t *id) {
		auto bf_status = table->dataFieldIdGet(name, id);
		ASSERT_BF_STATUS(bf_status, "Failed to configure table data %s",
						 name.c_str())
	}

	void init_data(
		const std::unordered_map<std::string, bf_rt_id_t *> &fields) {
		for (const auto &field : fields) {
			init_data(field.first, field.second);
		}
	}

	void init_data_with_action(bf_rt_id_t action_id) {
		std::vector<bf_rt_id_t> data_fields_ids;

		auto bf_status = table->dataFieldIdListGet(action_id, &data_fields_ids);
		ASSERT_BF_STATUS(bf_status, "Failed to get data field list")

		for (auto id : data_fields_ids) {
			std::string name;
			bits_t size;

			bf_status = table->dataFieldNameGet(id, action_id, &name);
			ASSERT_BF_STATUS(bf_status, "Failed to get data field name")

			bf_status = table->dataFieldSizeGet(id, action_id, &size);
			ASSERT_BF_STATUS(bf_status, "Failed to get data field size")

			data_fields.push_back({name, id, size});
		}

		LOG_DEBUG("Data:\n");
		for (auto d : data_fields) {
			LOG_DEBUG("  %s (%lu bits)\n", d.name.c_str(), d.size);
		}
	}

	void init_data_with_action(const std::string &name, bf_rt_id_t action_id,
							   bf_rt_id_t *field_id) {
		auto bf_status = table->dataFieldIdGet(name, action_id, field_id);
		ASSERT_BF_STATUS(bf_status,
						 "Failed to configure data table with actions %s",
						 name.c_str())
	}

	void init_data_with_actions(
		const std::unordered_map<std::string,
								 std::pair<bf_rt_id_t, bf_rt_id_t *>> &fields) {
		for (const auto &field : fields) {
			init_data_with_action(field.first, field.second.first,
								  field.second.second);
		}
	}

	void init_action(const std::string &name, bf_rt_id_t *action_id) {
		auto bf_status = table->actionIdGet(name, action_id);
		ASSERT_BF_STATUS(bf_status, "Failed to configure action %s",
						 name.c_str())
	}

	void init_actions(
		const std::unordered_map<std::string, bf_rt_id_t *> &actions) {
		for (const auto &action : actions) {
			init_action(action.first, action.second);
		}
	}

	void set_notify_mode(time_ms_t timeout_value, void *cookie,
						 const bfrt::BfRtIdleTmoExpiryCb &callback,
						 bool enable) {
		std::unique_ptr<bfrt::BfRtTableAttributes> attr;

		auto bf_status = table->attributeAllocate(
			bfrt::TableAttributesType::IDLE_TABLE_RUNTIME,
			bfrt::TableAttributesIdleTableMode::NOTIFY_MODE, &attr);

		ASSERT_BF_STATUS(bf_status, "Failed to allocate attribute for table %s",
						 table_name.c_str())

		uint32_t min_ttl = timeout_value;
		uint32_t max_ttl = timeout_value;
		uint32_t ttl_query_interval = timeout_value;

		assert(ttl_query_interval <= min_ttl);

		bf_status = attr->idleTableNotifyModeSet(
			enable, callback, ttl_query_interval, max_ttl, min_ttl, cookie);
		ASSERT_BF_STATUS(bf_status, "Failed to set notify mode for table %s",
						 table_name.c_str())

		uint32_t flags = 0;
		bf_status =
			table->tableAttributesSet(*session, dev_tgt, flags, *attr.get());
		ASSERT_BF_STATUS(bf_status, "Failed to set attributes for table %s",
						 table_name.c_str())
	}

public:
	size_t get_size() const {
		size_t size;
		auto bf_status = table->tableSizeGet(*session, dev_tgt, &size);
		ASSERT_BF_STATUS(bf_status, "Failed to get size of table %s",
						 table_name.c_str())
		return size;
	}

	size_t get_usage() const {
		uint32_t usage;
		auto bf_status = table->tableUsageGet(
			*session, dev_tgt, bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW,
			&usage);
		ASSERT_BF_STATUS(bf_status, "Failed to get usage of table %s",
						 table_name.c_str())
		return usage;
	}

	const std::vector<field_t> &get_key_fields() const { return key_fields; }
	const std::vector<field_t> &get_data_fields() const { return data_fields; }

	void dump_data_fields();
	void dump_data_fields(std::ostream &);

	void dump();
	void dump(std::ostream &);

	static void dump_table_names(const bfrt::BfRtInfo *bfrtInfo);
};

class Port_HDL_Info : TofinoTable {
private:
	// Key fields IDs
	bf_rt_id_t CONN_ID;
	bf_rt_id_t CHNL_ID;

	// Data field ids
	bf_rt_id_t DEV_PORT;

public:
	Port_HDL_Info() : TofinoTable("$PORT_HDL_INFO") {
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

class Port_Stat : TofinoTable {
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
	Port_Stat() : TofinoTable("$PORT_STAT") {
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
			{"$FramesReceivedoftypePAUSE",
			 &data_fields.FramesReceivedoftypePAUSE},
			{"$FramesReceivedwithLengthError",
			 &data_fields.FramesReceivedwithLengthError},
			{"$FramesReceivedUndersized",
			 &data_fields.FramesReceivedUndersized},
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
			{"$FramesReceivedLength_9216",
			 &data_fields.FramesReceivedLength_9216},
			{"$FramesTransmittedOK", &data_fields.FramesTransmittedOK},
			{"$FramesTransmittedAll", &data_fields.FramesTransmittedAll},
			{"$FramesTransmittedwithError",
			 &data_fields.FramesTransmittedwithError},
			{"$OctetsTransmittedwithouterror",
			 &data_fields.OctetsTransmittedwithouterror},
			{"$OctetsTransmittedTotal", &data_fields.OctetsTransmittedTotal},
			{"$FramesTransmittedUnicast",
			 &data_fields.FramesTransmittedUnicast},
			{"$FramesTransmittedMulticast",
			 &data_fields.FramesTransmittedMulticast},
			{"$FramesTransmittedBroadcast",
			 &data_fields.FramesTransmittedBroadcast},
			{"$FramesTransmittedPause", &data_fields.FramesTransmittedPause},
			{"$FramesTransmittedPriPause",
			 &data_fields.FramesTransmittedPriPause},
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
			{"$TransmitPri0Pause1USCount",
			 &data_fields.TransmitPri0Pause1USCount},
			{"$TransmitPri1Pause1USCount",
			 &data_fields.TransmitPri1Pause1USCount},
			{"$TransmitPri2Pause1USCount",
			 &data_fields.TransmitPri2Pause1USCount},
			{"$TransmitPri3Pause1USCount",
			 &data_fields.TransmitPri3Pause1USCount},
			{"$TransmitPri4Pause1USCount",
			 &data_fields.TransmitPri4Pause1USCount},
			{"$TransmitPri5Pause1USCount",
			 &data_fields.TransmitPri5Pause1USCount},
			{"$TransmitPri6Pause1USCount",
			 &data_fields.TransmitPri6Pause1USCount},
			{"$TransmitPri7Pause1USCount",
			 &data_fields.TransmitPri7Pause1USCount},
			{"$ReceivePri0Pause1USCount",
			 &data_fields.ReceivePri0Pause1USCount},
			{"$ReceivePri1Pause1USCount",
			 &data_fields.ReceivePri1Pause1USCount},
			{"$ReceivePri2Pause1USCount",
			 &data_fields.ReceivePri2Pause1USCount},
			{"$ReceivePri3Pause1USCount",
			 &data_fields.ReceivePri3Pause1USCount},
			{"$ReceivePri4Pause1USCount",
			 &data_fields.ReceivePri4Pause1USCount},
			{"$ReceivePri5Pause1USCount",
			 &data_fields.ReceivePri5Pause1USCount},
			{"$ReceivePri6Pause1USCount",
			 &data_fields.ReceivePri6Pause1USCount},
			{"$ReceivePri7Pause1USCount",
			 &data_fields.ReceivePri7Pause1USCount},
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

class Ports : TofinoTable {
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
	Ports() : TofinoTable("$PORT"), port_hdl_info(), port_stat() {
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
			{BF_SPEED_40G, "BF_SPEED_40G"},	  {BF_SPEED_50G, "BF_SPEED_50G"},
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
		auto dev_port =
			port_hdl_info.get_dev_port(front_panel_port, lane, false);
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

		auto bf_status =
			key->setValue(DEV_PORT, static_cast<uint64_t>(dev_port));
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

	fields_values_t() : size(0), values(nullptr) {}

	fields_values_t(uint32_t _size)
		: size(_size), values(new field_value_t[_size]) {}

	fields_values_t(const fields_values_t &key)
		: size(key.size), values(new field_value_t[key.size]) {
		for (auto i = 0u; i < size; i++) {
			values[i] = key.values[i];
		}
	}

	~fields_values_t() {
		if (values) delete values;
		size = 0;
	}

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

inline std::ostream &operator<<(std::ostream &os,
								const fields_values_t &fields_values) {
	os << "{";
	for (int i = 0u; i < fields_values.size; i++) {
		if (i > 0) os << ",";
		os << "0x" << std::hex << fields_values.values[i] << std::dec;
	}
	os << "}";
	return os;
}

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

	bytes_t(uint32_t _size, uint64_t value) : bytes_t(_size) {
		for (auto i = 0u; i < _size; i++) {
			values[i] = (value >> (8 * (_size - i - 1))) & 0xff;
		}
	}

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
		if (i > 0) os << ",";
		os << "0x" << std::hex << (int)bytes[i] << std::dec;
	}
	os << "}";
	return os;
}

bf_status_t idletime_callback(const bf_rt_target_t &target,
							  const bfrt::BfRtTableKey *key,
							  const void *cookie);

class Table;

struct cookie_t {
	Table *table;
};

struct time_cfg_t {
	bool valid;
	time_ms_t value;

	time_cfg_t() : valid(false), value(0) {}
	time_cfg_t(time_ms_t _value) : valid(true), value(_value) {}
};

typedef fields_values_t table_key_t;
typedef fields_values_t table_data_t;

struct table_entry_t {
	table_key_t key;
	table_data_t data;

	table_entry_t(const table_key_t &_key) : key(_key) {}

	table_entry_t(const table_key_t &_key, const table_data_t &_data)
		: key(_key), data(_data) {}

	bool operator==(const table_entry_t &other) { return key == other.key; }

	struct hash {
		std::size_t operator()(const table_entry_t &entry) const {
			return fields_values_t::hash()(entry.key);
		}
	};
};

bool operator==(const table_entry_t &lhs, const table_entry_t &rhs) {
	return lhs.key == rhs.key;
}

struct entries_t {
	std::unordered_set<table_entry_t, table_entry_t::hash> entries;

	void clear() { entries.clear(); }

	void set(const entries_t &other) { entries = other.entries; }

	void assert_entries() const {
		for (const auto &entry : entries) {
			assert(entry.key.size > 0);
		}
	}

	void add(const table_entry_t &entry) {
		assert(!contains(entry.key));
		entries.insert(entry);
		assert_entries();
	}

	void add(const table_key_t &key, const table_data_t &data) {
		assert(!contains(key));
		entries.emplace(key, data);
		assert_entries();
	}

	void add(const table_key_t &key) {
		assert(!contains(key));
		entries.emplace(key);
		assert_entries();
	}

	void erase(const table_key_t &key) {
		assert(contains(key));
		table_entry_t entry(key);
		entries.erase(entry);
	}

	bool contains(const table_key_t &key) const {
		table_entry_t entry(key);
		return entries.find(entry) != entries.end();
	}
};

class Table : public TofinoTable {
private:
	entries_t added_entries;
	entries_t snapshot_data;

	bf_rt_id_t populate_action_id;
	time_cfg_t timeout;
	cookie_t cookie;

public:
	Table(const std::string &_table_name)
		: TofinoTable("Ingress." + _table_name), cookie({this}) {
		init_key();
		init_action("Ingress.populate", &populate_action_id);
		init_data_with_action(populate_action_id);
	}

	Table(const std::string &_table_name, time_ms_t _timeout)
		: Table(_table_name) {
		timeout = time_cfg_t(_timeout);
		set_notify_mode(timeout.value, &cookie, idletime_callback, false);
	}

	void enable_expirations() {
		assert(timeout.valid);
		LOG_DEBUG("Enabling expirations on table %s...\n", table_name.c_str());
		set_notify_mode(timeout.value, &cookie, idletime_callback, true);
		LOG_DEBUG("Expirations enabled on table %s\n", table_name.c_str());
	}

	void disable_expirations() {
		assert(timeout.valid);
		LOG_DEBUG("Disabling expirations on table %s...\n", table_name.c_str());
		set_notify_mode(timeout.value, &cookie, idletime_callback, false);
		LOG_DEBUG("Expirations disabled on table %s...\n", table_name.c_str());
	}

	void snapshot() { snapshot_data.set(added_entries); }

	void rollback() {
		// disable_expirations(); // FIXME: enable this

		auto i = 1u;
		for (const auto &entry : added_entries.entries) {
			assert(entry.key.size);
			del(entry.key);
			LOG_DEBUG("Del %d/%lu\r", (i++), added_entries.entries.size());
		}
		LOG_DEBUG("\n");

		added_entries.clear();

		for (const auto &entry : snapshot_data.entries) {
			add(entry.key, entry.data);
			added_entries.add(entry);
		}
	}

	void set(const bytes_t &key_bytes, std::vector<bytes_t> params_bytes) {
		if (timeout.valid) {
			params_bytes.emplace_back(4, timeout.value);
		}

		auto key = bytes_to_fields_values(key_bytes, key_fields);
		auto data = bytes_to_fields_values(params_bytes, data_fields);

		if (!added_entries.contains(key)) {
			add(key, data);
			added_entries.add(key, data);
		}
	}

	void set(const bytes_t &key_bytes) {
		if (timeout.valid) {
			set(key_bytes, {});
			return;
		}

		auto key = bytes_to_fields_values(key_bytes, key_fields);

		if (!added_entries.contains(key)) {
			add(key);
			added_entries.add(key);
		}
	}

	void del(const bytes_t &key_bytes) {
		auto key = bytes_to_fields_values(key_bytes, key_fields);

		if (added_entries.contains(key)) {
			del(key);
			added_entries.erase(key);
		}
	}

	void del(const bfrt::BfRtTableKey *_key) {
		auto key = key_to_fields_values(_key);

		if (added_entries.contains(key)) {
			del(key);
			added_entries.erase(key);
		}
	}

	static std::unique_ptr<Table> build(const std::string &_table_name) {
		return std::unique_ptr<Table>(new Table(_table_name));
	}

	static std::unique_ptr<Table> build(const std::string &_table_name,
										time_ms_t _timeout) {
		return std::unique_ptr<Table>(new Table(_table_name, _timeout));
	}

private:
	void key_setup() {
		auto bf_status = table->keyReset(key.get());
		assert(bf_status == BF_SUCCESS);
	}

	void key_setup(const table_key_t &key_field_values) {
		key_setup();

		for (auto i = 0u; i < key_fields.size(); i++) {
			auto key_field_value = key_field_values.values[i];
			auto key_field = key_fields[i];
			auto bf_status = key->setValue(key_field.id, key_field_value);
			assert(bf_status == BF_SUCCESS);
		}
	}

	void data_setup() {
		auto bf_status = table->dataReset(populate_action_id, data.get());
		assert(bf_status == BF_SUCCESS);
	}

	void data_setup(const table_data_t &param_field_values) {
		data_setup();

		assert(param_field_values.size <= data_fields.size());

		for (auto i = 0u; i < param_field_values.size; i++) {
			auto param_field_value = param_field_values.values[i];
			auto param_field = data_fields[i];
			auto bf_status = data->setValue(param_field.id, param_field_value);
			assert(bf_status == BF_SUCCESS);
		}
	}

	table_data_t get(const table_key_t &key_fields_values) {
		dump(key_fields_values, table_data_t(), "GET");

		key_setup(key_fields_values);
		data_setup();

		uint64_t flags = 0;
		BF_RT_FLAG_CLEAR(flags, BF_RT_FROM_HW);

		auto bf_status =
			table->tableEntryGet(*session, dev_tgt, flags, *key, data.get());
		ASSERT_BF_STATUS(bf_status, "Failed to get entry from table %s",
						 table_name.c_str())

		auto data_fields = data_to_fields_values(data.get());
		return data_fields;
	}

	void add(const table_key_t &key_fields_values,
			 const table_data_t &param_fields_values) {
		dump(key_fields_values, param_fields_values, "ADD");

		key_setup(key_fields_values);
		data_setup(param_fields_values);

		uint64_t flags = 0;
		BF_RT_FLAG_CLEAR(flags, BF_RT_FROM_HW);

		auto bf_status =
			table->tableEntryAdd(*session, dev_tgt, flags, *key, *data);
		if (bf_status != BF_SUCCESS) {
			printf("inspect\n");
			fflush(stdout);
			sleep(100000000);
		}
		ASSERT_BF_STATUS(bf_status, "Failed to add entry to table %s",
						 table_name.c_str())
	}

	void add(const table_key_t &key_fields_values) {
		dump(key_fields_values, table_data_t(), "ADD");

		key_setup(key_fields_values);
		data_setup();

		uint64_t flags = 0;
		BF_RT_FLAG_CLEAR(flags, BF_RT_FROM_HW);

		auto bf_status =
			table->tableEntryAdd(*session, dev_tgt, flags, *key, *data);
		ASSERT_BF_STATUS(bf_status, "Failed to add entry to table %s",
						 table_name.c_str())
	}

	void mod(const table_key_t &key_fields_values,
			 const table_data_t &param_fields_values) {
		dump(key_fields_values, table_data_t(), "MOD");

		key_setup(key_fields_values);
		data_setup(param_fields_values);

		uint64_t flags = 0;
		BF_RT_FLAG_CLEAR(flags, BF_RT_FROM_HW);

		auto bf_status =
			table->tableEntryMod(*session, dev_tgt, flags, *key, *data);
		ASSERT_BF_STATUS(bf_status, "Failed to modify entry of table %s",
						 table_name.c_str())
	}

	void del(const table_key_t &key_fields_values) {
		dump(key_fields_values, table_data_t(), "DEL");

		key_setup(key_fields_values);

		uint64_t flags = 0;
		BF_RT_FLAG_CLEAR(flags, BF_RT_FROM_HW);

		auto bf_status = table->tableEntryDel(*session, dev_tgt, flags, *key);
		ASSERT_BF_STATUS(bf_status, "Failed to delete entry of table %s",
						 table_name.c_str())
	}

	void clear() {
		uint64_t flags = 0;
		BF_RT_FLAG_CLEAR(flags, BF_RT_FROM_HW);

		auto bf_status = table->tableClear(*session, dev_tgt, flags);
		ASSERT_BF_STATUS(bf_status, "Failed to clear table %s",
						 table_name.c_str())

#ifndef NDEBUG
		LOG_DEBUG("\n");
		LOG_DEBUG("*********************************************\n");
		LOG_DEBUG("Time: %lu\n", time(0));
		LOG_DEBUG("[%s] Clear\n", table_name.c_str());
		LOG_DEBUG("*********************************************\n");
#endif
	}

	void dump(const table_key_t &key, const table_data_t &data,
			  const std::string &op) const {
		LOG_DEBUG("\n");
		LOG_DEBUG("*********************************************\n");

		assert(key.size > 0);
		assert(key.size == key_fields.size());

		LOG_DEBUG("Time: %lu\n", time(0));
		LOG_DEBUG("[%s] Op: %s\n", table_name.c_str(), op.c_str());

		LOG_DEBUG("  Key:\n");
		for (auto i = 0u; i < key.size; i++) {
			auto key_field = key_fields[i];
			auto key_value = key.values[i];
			LOG_DEBUG("     %s : 0x%02lx\n", key_field.name.c_str(), key_value);
		}

		if (data.size) {
			assert(data.size <= data_fields.size());

			LOG_DEBUG("  Data:\n");
			for (auto i = 0u; i < data.size; i++) {
				auto data_field = data_fields[i];
				auto data_value = data.values[i];
				LOG_DEBUG("     %s : 0x%02lx\n", data_field.name.c_str(),
						  data_value);
			}
		}

		LOG_DEBUG("*********************************************\n");
	}

public:
	table_key_t key_to_fields_values(const bfrt::BfRtTableKey *key) {
		auto fields_values = table_key_t(key_fields.size());

		for (auto i = 0u; i < key_fields.size(); i++) {
			auto field = key_fields[i];
			auto bf_status = key->getValue(field.id, &fields_values.values[i]);
			ASSERT_BF_STATUS(bf_status, "Failed to get value from key")
		}

		return fields_values;
	}

	table_data_t data_to_fields_values(const bfrt::BfRtTableData *data) {
		auto fields_values = table_data_t(data_fields.size());

		for (auto i = 0u; i < data_fields.size(); i++) {
			auto field = data_fields[i];
			auto bf_status = data->getValue(field.id, &fields_values.values[i]);
			ASSERT_BF_STATUS(bf_status, "Failed to get value from data")
		}

		return fields_values;
	}

	static fields_values_t bytes_to_fields_values(
		const bytes_t &values, const std::vector<field_t> &fields) {
		auto fields_values = fields_values_t(fields.size());
		auto starting_byte = 0;

		for (auto kf_i = 0u; kf_i < fields.size(); kf_i++) {
			auto kf = fields[kf_i];
			auto bytes = kf.size / 8;

			assert(values.size >= starting_byte + bytes);

			fields_values.values[kf_i] = 0;

			for (auto i = 0u; i < bytes; i++) {
				auto byte = values[starting_byte + i];

				fields_values.values[kf_i] =
					(fields_values.values[kf_i] << 8) | byte;
			}

			starting_byte += bytes;
		}

		return fields_values;
	}

	// one to one relation
	static fields_values_t bytes_to_fields_values(
		const std::vector<bytes_t> &values,
		const std::vector<field_t> &fields) {
		auto fields_values = fields_values_t(values.size());

		assert(fields.size() >= values.size());

		for (auto i = 0u; i < values.size(); i++) {
			auto value = values[i];
			auto field = fields[i];
			auto bytes = field.size / 8;

			assert(value.size == bytes);

			fields_values.values[i] = 0;

			for (auto j = 0u; j < bytes; j++) {
				fields_values.values[i] =
					(fields_values.values[i] << 8) | value[j];
			}
		}

		return fields_values;
	}
};

bf_status_t idletime_callback(const bf_rt_target_t &target,
							  const bfrt::BfRtTableKey *key,
							  const void *cookie) {
	auto c = (cookie_t *)cookie;
	auto table = c->table;

	begin_transaction();
	LOG_DEBUG("expiring...\n");
	table->del(key);
	LOG_DEBUG("expired!\n");
	end_transaction();

	return BF_SUCCESS;
}

template <typename T>
class Map {
private:
	std::unordered_map<bytes_t, T, bytes_t::hash> data;
	std::vector<std::unique_ptr<Table>> tables;
	std::unique_ptr<Table> timeout_table;

public:
	Map() {}

	Map(const std::vector<std::string> &tables_names) {
		for (const auto &table_name : tables_names) {
			tables.push_back(Table::build(table_name));
		}
	}

	Map(const std::vector<std::string> &tables_names,
		const std::string &timeout_table_name, time_ms_t timeout)
		: Map(tables_names) {
		timeout_table = Table::build(timeout_table_name, timeout);
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
		for (auto &table : tables) {
			table->set(key, value_bytes);
		}
	}

	static std::unique_ptr<Map> build() {
		return std::unique_ptr<Map>(new Map());
	}

	static std::unique_ptr<Map> build(
		const std::vector<std::string> &tables_names) {
		return std::unique_ptr<Map>(new Map(tables_names));
	}

	static std::unique_ptr<Map> build(
		const std::vector<std::string> &tables_names,
		const std::string &timeout_table_name, time_ms_t timeout) {
		return std::unique_ptr<Map>(
			new Map(tables_names, timeout_table_name, timeout));
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

void configure_ports(const std::vector<int> &switch_ports,
					 bool are_front_panel) {
	Ports ports;

	auto to_dev_port = [&](int port) {
		return are_front_panel ? ports.get_dev_port(port, 0) : port;
	};

	for (const auto &port : switch_ports) {
		// Let's assume they are all 100G ports
		auto speed = Ports::gbps_to_bf_port_speed(100);
		auto dev_port = to_dev_port(port);
		ports.add_dev_port(dev_port, speed);
	}

	for (const auto &port : switch_ports) {
		auto dev_port = to_dev_port(port);
		LOG("Waiting for port %d\n", dev_port);
		while (!ports.is_port_up(to_dev_port(port))) {
			sleep(1);
		}
	}
}

#define IN_PORT 5
#define OUT_PORT 6
#define IN_DEV_PORT 164
#define OUT_DEV_PORT 172

void wait_for_cli() {
	pthread_join(switchd_ctx->tmr_t_id, NULL);
	pthread_join(switchd_ctx->dma_t_id, NULL);
	pthread_join(switchd_ctx->int_t_id, NULL);
	pthread_join(switchd_ctx->pkt_t_id, NULL);
	pthread_join(switchd_ctx->port_fsm_t_id, NULL);
	pthread_join(switchd_ctx->drusim_t_id, NULL);
	for (size_t i = 0;
		 i < sizeof switchd_ctx->agent_t_id / sizeof switchd_ctx->agent_t_id[0];
		 ++i) {
		if (switchd_ctx->agent_t_id[i] != 0) {
			pthread_join(switchd_ctx->agent_t_id[i], NULL);
		}
	}

	free(switchd_ctx);
}

struct state_t;
std::unique_ptr<state_t> state;

struct args_t {
	bool hardware_mode;
	bool bench_mode;
	bool switchd_background;
	bool wait_to_enable_timeouts;
	time_ms_t timeout;

	args_t(int argc, char **argv)
		: hardware_mode(false),
		  bench_mode(false),
		  switchd_background(false),
		  wait_to_enable_timeouts(false) {
		if (argc <= 1) {
			help();
			exit(1);
		}

		auto curr = 1;
		parse_timeout(argc, argv, curr);

		auto match = true;
		while (match && curr < argc) {
			match = false;
			match |= try_parse_hw(argc, argv, curr);
			match |= try_parse_bench(argc, argv, curr);
			match |= try_parse_switchd_background(argc, argv, curr);
			match |= try_parse_wait_to_enable_timeouts(argc, argv, curr);
		}

		if (!switchd_background && wait_to_enable_timeouts) {
			LOG("Can't have user enabled timeouts without sending the switchd "
				"to the background.\n");
			exit(1);
		}
	}

	void parse_timeout(int argc, char **argv, int &curr) {
		if (curr >= argc) {
			help();
			exit(1);
		}

		try {
			auto arg = argv[curr];
			timeout = std::stoi(arg);
			curr++;
		} catch (std::invalid_argument e) {
			help();
			exit(1);
		}
	}

	bool try_parse_hw(int argc, char **argv, int &curr) {
		return try_parse_flag(argc, argv, curr, "--hw", hardware_mode);
	}

	bool try_parse_bench(int argc, char **argv, int &curr) {
		return try_parse_flag(argc, argv, curr, "--bench", bench_mode);
	}

	bool try_parse_switchd_background(int argc, char **argv, int &curr) {
		return try_parse_flag(argc, argv, curr, "--switchd-background",
							  switchd_background);
	}

	bool try_parse_wait_to_enable_timeouts(int argc, char **argv, int &curr) {
		return try_parse_flag(argc, argv, curr, "--user-enables-timeouts",
							  wait_to_enable_timeouts);
	}

	bool try_parse_flag(int argc, char **argv, int &curr,
						const std::string &flag, bool &out) {
		if (curr >= argc) {
			return false;
		}

		auto arg = argv[curr];

		if (std::string(arg) == flag) {
			out = true;
			curr++;
			return true;
		}

		return false;
	}

	void help() const {
		LOG("Usage: timeouts timeout [--hw] [--bench] "
			"[--user-enables-timeouts]\n");
		LOG(" timeout:\n");
		LOG("    Timeout value (in ms)\n");
		LOG(" --hw:\n");
		LOG("    Run on the switch hardware\n");
		LOG(" --bench:\n");
		LOG("    Enter bench mode.\n");
		LOG("      1. switchd runs on the background.\n");
		LOG("      2. controller starts in warmup mode, filling up dataplane "
			"tables according to incoming traffic (with timeouts disabled).\n");
		LOG("      3. press <enter> to enter bench mode, which makes the "
			"controller take a snapshot of the tables. It warns the user when "
			"the snapshot is completed, and is ready for receiving packets. "
			"Press <enter> again to enable timeouts.\n");
		LOG("      4. press <enter> to rollback the state to the snapshot "
			"state taken right after the warmup. Timeouts are disabled when "
			"performing the snapshot, and enabled \n");
		LOG(" --user-enables-timeouts:\n");
		LOG("    Table timeouts are enabled through stdin\n");
	}

	void print() const {
		LOG("\n");
		LOG("Hardware:              %d\n", hardware_mode);
		LOG("Bench:                 %d\n", bench_mode);
		LOG("Switchd backgr:        %d\n", switchd_background);
		LOG("User enabled timeouts: %d\n", wait_to_enable_timeouts);
		LOG("Timeout:               %ld ms\n", timeout);
		LOG("\n");
	}
};

struct state_t {
  /*@{NF STATE DECL}@*/

  state_t(time_ms_t timeout) {
    /*@{NF STATE INIT}@*/
  }

  void enable_expirations(bool lock = true) {
		if (lock) {
			begin_transaction();
		}
		LOG_DEBUG("Enabling timeouts...\n");
		
    // TODO:

		LOG_DEBUG("Timeouts enabled\n");
		if (lock) {
			end_transaction();
		}
	}

	void disable_expirations(bool lock = true) {
		begin_transaction();
		LOG_DEBUG("Disabling timeouts...\n");

		// TODO:

		LOG_DEBUG("Timeouts disabled\n");
		end_transaction();
	}

	void snapshot() {
		LOG_DEBUG("Making a snapshot...\n");
		
    // TODO:

		LOG_DEBUG("Snapshot completed\n");
	}

	void rollback() {
		begin_batch();
		LOG_DEBUG("Rolling back...\n");

		// TODO:

		LOG_DEBUG("Rollback completed\n");
		end_batch();
	}
};

void wait_and_enable_timeouts() {
	WAIT_FOR_ENTER("Press <Enter> to enable timeouts...")
	state->enable_expirations();
}

void state_init(time_ms_t timeout) {
	state = std::unique_ptr<state_t>(new state_t(timeout));
}

void warmup_mode() { LOG("Current state: warmup\n"); }

void snapshot_mode() {
	LOG("Current state: snapshot\n");
	state->snapshot();
}

void rollback_mode() {
	LOG("Current state: rollback\n");
	state->rollback();
}

void bench_mode() {
	LOG("Current state: bench\n");
	state->enable_expirations();
}

void bench(const args_t args) {
	setup_transaction();
	init_bf_switchd(PROGRAM, true);
	setup_bf_session(PROGRAM);

	if (args.hardware_mode) {
		configure_ports({IN_DEV_PORT, OUT_DEV_PORT}, false);
	}

	register_pcie_pkt_ops();
	state_init(args.timeout);

	LOG_DEBUG("\n");
	LOG_DEBUG("********************************************************\n");
	LOG_DEBUG("*                                                      *\n");
	LOG_DEBUG("*                      WARNING                         *\n");
	LOG_DEBUG("*  Running in bench mode but compiled with debug flags *\n");
	LOG_DEBUG("*                                                      *\n");
	LOG_DEBUG("********************************************************\n");

	args.print();

	LOG("Controller is ready.\n");

	warmup_mode();

	WAIT_FOR_ENTER("Press <Enter> to enter snapshot state...")
	snapshot_mode();

	while (true) {
		WAIT_FOR_ENTER("Press <Enter> to enter bench state...")
		bench_mode();

		WAIT_FOR_ENTER("Press <Enter> to enter rollback state...")
		rollback_mode();
	}

	wait_for_cli();
}

int main(int argc, char **argv) {
	args_t args(argc, argv);

	if (args.bench_mode) {
		bench(args);
		return 0;
	}

	setup_transaction();
	init_bf_switchd(PROGRAM, args.switchd_background);
	setup_bf_session(PROGRAM);

	if (args.hardware_mode) {
		configure_ports({IN_DEV_PORT, OUT_DEV_PORT}, false);
	}

	register_pcie_pkt_ops();
	state_init(args.timeout);

	if (!args.wait_to_enable_timeouts) {
		state->enable_expirations();
	}

	LOG("\n\n");
	args.print();

	LOG("Controller is ready.\n");

	if (args.wait_to_enable_timeouts) {
		wait_and_enable_timeouts();
	}

	wait_for_cli();

	return 0;
}

bool nf_process(time_ns_t now, pkt_t &pkt) {
  /*@{NF PROCESS}@*/
}