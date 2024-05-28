#pragma once

#include "../target.h"

// #include "broadcast.h"
// #include "cht_find_backend.h"
// #include "dchain_allocate_new_index.h"
// #include "dchain_free_index.h"
// #include "dchain_is_index_allocated.h"
// #include "dchain_rejuvenate_index.h"
// #include "drop.h"
#include "if.h"
#include "then.h"
#include "else.h"
// #include "expire_items_single_map.h"
// #include "expire_items_single_map_iteratively.h"
// #include "forward.h"
// #include "hash_obj.h"
// #include "load_balanced_flow_hash.h"
// #include "map_erase.h"
// #include "map_get.h"
// #include "map_put.h"
// #include "nf_set_rte_ipv4_udptcp_checksum.h"
// #include "packet_borrow_next_chunk.h"
// #include "packet_return_chunk.h"
// #include "sketch_compute_hashes.h"
// #include "sketch_expire.h"
// #include "sketch_fetch.h"
// #include "sketch_refresh.h"
// #include "sketch_touch_buckets.h"
// #include "vector_borrow.h"
// #include "vector_return.h"

#include "x86_context.h"

namespace synapse {
namespace x86 {

struct x86Target : public Target {
  x86Target()
      : Target(TargetType::x86,
               {
                   //    new PacketBorrowNextChunkGenerator(),
                   //    new PacketReturnChunkGenerator(),
                   new IfGenerator(), new ThenGenerator(), new ElseGenerator(),
                   //    new ForwardGenerator(),
                   //    new BroadcastGenerator(),
                   //    new DropGenerator(),
                   //    new ExpireItemsSingleMapGenerator(),
                   //    new ExpireItemsSingleMapIterativelyGenerator(),
                   //    new DchainRejuvenateIndexGenerator(),
                   //    new VectorBorrowGenerator(),
                   //    new VectorReturnGenerator(),
                   //    new DchainAllocateNewIndexGenerator(),
                   //    new DchainFreeIndexGenerator(),
                   //    new MapGetGenerator(),
                   //    new MapPutGenerator(),
                   //    new SetIpv4UdpTcpChecksumGenerator(),
                   //    new DchainIsIndexAllocatedGenerator(),
                   //    new SketchComputeHashesGenerator(),
                   //    new SketchExpireGenerator(),
                   //    new SketchFetchGenerator(),
                   //    new SketchRefreshGenerator(),
                   //    new SketchTouchBucketsGenerator(),
                   //    new MapEraseGenerator(),
                   //    new LoadBalancedFlowHashGenerator(),
                   //    new ChtFindBackendGenerator(),
                   //    new HashObjGenerator(),
               },
               new x86Context()) {}
};

} // namespace x86
} // namespace synapse
