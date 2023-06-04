#pragma once

#include "../../target.h"
#include "../module.h"

#include "dchain_allocate_new_index.h"
#include "dchain_free_index.h"
#include "dchain_is_index_allocated.h"
#include "dchain_rejuvenate_index.h"
#include "drop.h"
#include "else.h"
#include "ether_addr_hash.h"
#include "forward_through_tofino.h"
#include "hash_obj.h"
#include "if.h"
#include "ignore.h"
#include "map_erase.h"
#include "map_get.h"
#include "map_put.h"
#include "memory_bank.h"
#include "packet_modify_checksums.h"
#include "packet_modify_ethernet.h"
#include "packet_modify_ipv4.h"
#include "packet_modify_ipv4_options.h"
#include "packet_modify_tcp.h"
#include "packet_modify_tcpudp.h"
#include "packet_modify_udp.h"
#include "packet_parse_cpu.h"
#include "packet_parse_ethernet.h"
#include "packet_parse_ipv4.h"
#include "packet_parse_ipv4_options.h"
#include "packet_parse_tcp.h"
#include "packet_parse_tcpudp.h"
#include "packet_parse_udp.h"
#include "then.h"

namespace synapse {
namespace targets {
namespace x86_tofino {

class x86TofinoTarget : public Target {
public:
  x86TofinoTarget()
      : Target(TargetType::x86_Tofino,
               {
                   MODULE(Ignore),
                   MODULE(PacketParseCPU),
                   MODULE(PacketParseEthernet),
                   MODULE(PacketModifyEthernet),
                   MODULE(PacketParseIPv4),
                   MODULE(PacketModifyIPv4),
                   MODULE(PacketParseIPv4Options),
                   MODULE(PacketModifyIPv4Options),
                   MODULE(PacketParseTCPUDP),
                   MODULE(PacketModifyTCPUDP),
                   MODULE(PacketParseTCP),
                   MODULE(PacketModifyTCP),
                   MODULE(PacketParseUDP),
                   MODULE(PacketModifyUDP),
                   MODULE(PacketModifyChecksums),
                   MODULE(If),
                   MODULE(Then),
                   MODULE(Else),
                   MODULE(Drop),
                   MODULE(ForwardThroughTofino),
                   MODULE(MapGet),
                   MODULE(MapPut),
                   MODULE(MapErase),
                   MODULE(EtherAddrHash),
                   MODULE(DchainAllocateNewIndex),
                   MODULE(DchainIsIndexAllocated),
                   MODULE(DchainRejuvenateIndex),
                   MODULE(DchainFreeIndex),
                   MODULE(HashObj),
               },
               TargetMemoryBank_ptr(new x86TofinoMemoryBank())) {}

  static Target_ptr build() { return Target_ptr(new x86TofinoTarget()); }
};

} // namespace x86_tofino
} // namespace targets
} // namespace synapse
