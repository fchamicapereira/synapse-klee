#pragma once

#include "../target.h"

#include "ignore.h"
#include "parse_header.h"
#include "modify_header.h"
#include "if.h"
#include "then.h"
#include "else.h"
#include "forward.h"
#include "broadcast.h"
#include "drop.h"
#include "simple_table_lookup.h"
#include "simple_table_update.h"
#include "dchain_allocate_new_index.h"
#include "vector_read.h"
#include "vector_write.h"

#include "tofino_cpu_context.h"

namespace synapse {
namespace tofino_cpu {

struct TofinoCPUTarget : public Target {
  TofinoCPUTarget()
      : Target(TargetType::TofinoCPU,
               {
                   new IgnoreGenerator(),
                   new ParseHeaderGenerator(),
                   new ModifyHeaderGenerator(),
                   new IfGenerator(),
                   new ThenGenerator(),
                   new ElseGenerator(),
                   new ForwardGenerator(),
                   new BroadcastGenerator(),
                   new DropGenerator(),
                   new SimpleTableLookupGenerator(),
                   new SimpleTableUpdateGenerator(),
                   new DchainAllocateNewIndexGenerator(),
                   new VectorReadGenerator(),
                   new VectorWriteGenerator(),
               },
               new TofinoCPUContext()) {}
};

} // namespace tofino_cpu
} // namespace synapse
