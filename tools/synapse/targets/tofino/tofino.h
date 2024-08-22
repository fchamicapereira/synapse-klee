#pragma once

#include "../target.h"

#include "send_to_controller.h"
#include "recirculate.h"
#include "ignore.h"
#include "if_simple.h"
#include "if.h"
#include "then.h"
#include "else.h"
#include "forward.h"
#include "drop.h"
#include "broadcast.h"
#include "parser_extraction.h"
#include "parser_condition.h"
#include "parser_reject.h"
#include "modify_header.h"
#include "simple_table_lookup.h"
#include "vector_register_lookup.h"
#include "vector_register_update.h"
#include "fcfs_cached_table_read.h"
#include "fcfs_cached_table_read_or_write.h"
#include "fcfs_cached_table_write.h"
#include "fcfs_cached_table_delete.h"

#include "tofino_context.h"
#include "../../profiler.h"

namespace synapse {
namespace tofino {

struct TofinoTarget : public Target {
  TofinoTarget(TNAVersion version, const Profiler *profiler)
      : Target(TargetType::Tofino,
               {
                   new SendToControllerGenerator(),
                   new RecirculateGenerator(),
                   new IgnoreGenerator(),
                   new IfSimpleGenerator(),
                   new IfGenerator(),
                   new ThenGenerator(),
                   new ElseGenerator(),
                   new ForwardGenerator(),
                   new DropGenerator(),
                   new BroadcastGenerator(),
                   new ParserExtractionGenerator(),
                   new ParserConditionGenerator(),
                   new ParserRejectGenerator(),
                   new ModifyHeaderGenerator(),
                   new SimpleTableLookupGenerator(),
                   new VectorRegisterLookupGenerator(),
                   new VectorRegisterUpdateGenerator(),
                   new FCFSCachedTableReadGenerator(),
                   new FCFSCachedTableReadOrWriteGenerator(),
                   new FCFSCachedTableWriteGenerator(),
                   new FCFSCachedTableDeleteGenerator(),
               },
               new TofinoContext(version, profiler)) {}
};

} // namespace tofino
} // namespace synapse
