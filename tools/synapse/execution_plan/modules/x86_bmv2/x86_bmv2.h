#pragma once

#include "../../target.h"
#include "../module.h"

#include "broadcast.h"
#include "current_time.h"
#include "dchain_allocate_new_index.h"
#include "dchain_is_index_allocated.h"
#include "dchain_rejuvenate_index.h"
#include "drop.h"
#include "else.h"
#include "expire_items_single_map.h"
#include "forward.h"
#include "if.h"
#include "map_get.h"
#include "map_put.h"
#include "memory_bank.h"
#include "nf_set_rte_ipv4_udptcp_checksum.h"
#include "packet_borrow_next_chunk.h"
#include "packet_get_metadata.h"
#include "packet_get_unread_length.h"
#include "packet_return_chunk.h"
#include "rte_ether_addr_hash.h"
#include "then.h"
#include "vector_borrow.h"
#include "vector_return.h"

namespace synapse {
namespace targets {
namespace x86_bmv2 {

class x86BMv2Target : public Target {
public:
  x86BMv2Target()
      : Target(TargetType::x86_BMv2,
               {
                   MODULE(MapGet),
                   MODULE(CurrentTime),
                   MODULE(PacketBorrowNextChunk),
                   MODULE(PacketReturnChunk),
                   MODULE(PacketGetMetadata),
                   MODULE(If),
                   MODULE(Then),
                   MODULE(Else),
                   MODULE(Forward),
                   MODULE(Broadcast),
                   MODULE(Drop),
                   MODULE(ExpireItemsSingleMap),
                   MODULE(RteEtherAddrHash),
                   MODULE(DchainRejuvenateIndex),
                   MODULE(VectorBorrow),
                   MODULE(VectorReturn),
                   MODULE(DchainAllocateNewIndex),
                   MODULE(MapPut),
                   MODULE(PacketGetUnreadLength),
                   MODULE(SetIpv4UdpTcpChecksum),
                   MODULE(DchainIsIndexAllocated),
               },
               TargetMemoryBank_ptr(new x86BMv2MemoryBank())) {}

  static Target_ptr build() { return Target_ptr(new x86BMv2Target()); }
};

} // namespace x86_bmv2
} // namespace targets
} // namespace synapse
