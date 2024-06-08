#pragma once

#include "../target.h"

#include "send_to_controller.h"
#include "ignore.h"
#include "if.h"
#include "then.h"
#include "else.h"
#include "forward.h"
#include "drop.h"
#include "broadcast.h"
#include "parser_extraction.h"
#include "parser_condition.h"
#include "modify_header.h"
#include "simple_table_lookup.h"
#include "vector_register_lookup.h"
#include "vector_register_update.h"
#include "cached_table_read.h"
#include "cached_table_cond_write.h"
#include "cached_table_write.h"
#include "cached_table_delete.h"

#include "tofino_context.h"

namespace synapse {
namespace tofino {

struct TofinoTarget : public Target {
  TofinoTarget(TNAVersion version)
      : Target(TargetType::Tofino,
               {
                   new SendToControllerGenerator(),
                   new IgnoreGenerator(),
                   new IfGenerator(),
                   new ParserConditionGenerator(),
                   new ThenGenerator(),
                   new ElseGenerator(),
                   new ForwardGenerator(),
                   new DropGenerator(),
                   new BroadcastGenerator(),
                   new ParserExtractionGenerator(),
                   new ModifyHeaderGenerator(),
                   new SimpleTableLookupGenerator(),
                   new VectorRegisterLookupGenerator(),
                   new VectorRegisterUpdateGenerator(),
                   new CachedTableReadGenerator(),
                   new CachedTableConditionalWriteGenerator(),
                   new CachedTableWriteGenerator(),
                   new CachedTableDeleteGenerator(),
               },
               new TofinoContext(version)) {}
};

} // namespace tofino
} // namespace synapse
