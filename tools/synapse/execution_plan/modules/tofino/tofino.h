#pragma once

#include "../../target.h"
#include "../module.h"

#include "counter_increment.h"
#include "counter_read.h"
#include "drop.h"
#include "else.h"
#include "forward.h"
#include "hash_obj.h"
#include "if.h"
#include "ignore.h"
#include "int_allocator_allocate.h"
#include "int_allocator_query.h"
#include "int_allocator_rejuvenate.h"
#include "ipv4_tcpudp_checksums_update.h"
#include "memory_bank.h"
#include "modify_custom_header.h"
#include "parse_custom_header.h"
#include "parser_condition.h"
#include "send_to_controller.h"
#include "setup_expiration_notifications.h"
#include "table_is_allocated.h"
#include "table_lookup.h"
#include "table_rejuvenation.h"
#include "then.h"

namespace synapse {
namespace targets {
namespace tofino {

class TofinoTarget : public Target {
public:
  TofinoTarget()
      : Target(TargetType::Tofino,
               {
                   MODULE(Ignore),
                   MODULE(ParseCustomHeader),
                   MODULE(ModifyCustomHeader),
                   MODULE(ParserCondition),
                   MODULE(If),
                   MODULE(Then),
                   MODULE(Else),
                   MODULE(Forward),
                   MODULE(Drop),
                   MODULE(SendToController),
                   MODULE(SetupExpirationNotifications),
                   MODULE(TableLookup),
                   MODULE(TableRejuvenation),
                   MODULE(TableIsAllocated),
                   MODULE(IntegerAllocatorAllocate),
                   MODULE(IntegerAllocatorRejuvenate),
                   MODULE(IntegerAllocatorQuery),
                   MODULE(CounterRead),
                   MODULE(CounterIncrement),
                   MODULE(HashObj),
               },
               TargetMemoryBank_ptr(new TofinoMemoryBank())) {}

  static Target_ptr build() { return Target_ptr(new TofinoTarget()); }
};

} // namespace tofino
} // namespace targets
} // namespace synapse
