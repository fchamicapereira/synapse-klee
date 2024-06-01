#pragma once

#include "../target.h"

#include "if.h"
#include "then.h"
#include "else.h"
#include "forward.h"
#include "broadcast.h"
#include "drop.h"
#include "parse_header.h"
#include "modify_header.h"
#include "checksum_update.h"
#include "expire_items_single_map.h"
#include "expire_items_single_map_iteratively.h"
#include "map_get.h"
#include "map_put.h"
#include "map_erase.h"
#include "vector_read.h"
#include "vector_write.h"
#include "dchain_allocate_new_index.h"
#include "dchain_is_index_allocated.h"
#include "dchain_rejuvenate_index.h"
#include "dchain_free_index.h"
#include "sketch_compute_hashes.h"
#include "sketch_expire.h"
#include "sketch_fetch.h"
#include "sketch_refresh.h"
#include "sketch_touch_buckets.h"
#include "hash_obj.h"
#include "cht_find_backend.h"

#include "x86_context.h"

namespace synapse {
namespace x86 {

struct x86Target : public Target {
  x86Target()
      : Target(TargetType::x86,
               {
                   new ParserExtractionGenerator(),
                   new ModifyHeaderGenerator(),
                   new ChecksumUpdateGenerator(),
                   new IfGenerator(),
                   new ThenGenerator(),
                   new ElseGenerator(),
                   new ForwardGenerator(),
                   new BroadcastGenerator(),
                   new DropGenerator(),
                   new ExpireItemsSingleMapGenerator(),
                   new ExpireItemsSingleMapIterativelyGenerator(),
                   new MapGetGenerator(),
                   new MapPutGenerator(),
                   new MapEraseGenerator(),
                   new VectorReadGenerator(),
                   new VectorWriteGenerator(),
                   new DchainRejuvenateIndexGenerator(),
                   new DchainAllocateNewIndexGenerator(),
                   new DchainIsIndexAllocatedGenerator(),
                   new DchainFreeIndexGenerator(),
                   new SketchComputeHashesGenerator(),
                   new SketchExpireGenerator(),
                   new SketchFetchGenerator(),
                   new SketchRefreshGenerator(),
                   new SketchTouchBucketsGenerator(),
                   new HashObjGenerator(),
                   new ChtFindBackendGenerator(),
               },
               new x86Context()) {}
};

} // namespace x86
} // namespace synapse
