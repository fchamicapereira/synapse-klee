#pragma once

#include "../../target.h"
#include "../module.h"

#include "drop.h"
#include "else.h"
#include "ethernet_consume.h"
#include "ethernet_modify.h"
#include "forward.h"
#include "if.h"
#include "ignore.h"
#include "ip_options_consume.h"
#include "ip_options_modify.h"
#include "ipv4_consume.h"
#include "ipv4_modify.h"
#include "memory_bank.h"
#include "send_to_controller.h"
#include "setup_expiration_notifications.h"
#include "table_lookup.h"
#include "tcpudp_consume.h"
#include "tcpudp_modify.h"
#include "then.h"
#include "vector_return.h"

namespace synapse {
namespace targets {
namespace bmv2 {

class BMv2Target : public Target {
public:
  BMv2Target()
      : Target(TargetType::BMv2,
               {
                   MODULE(SendToController),
                   MODULE(Ignore),
                   MODULE(If),
                   MODULE(Then),
                   MODULE(Else),
                   MODULE(SetupExpirationNotifications),
                   MODULE(EthernetConsume),
                   MODULE(TableLookup),
                   MODULE(IPv4Consume),
                   MODULE(TcpUdpConsume),
                   MODULE(EthernetModify),
                   MODULE(IPv4Modify),
                   MODULE(Drop),
                   MODULE(Forward),
                   MODULE(VectorReturn),
                   MODULE(IPOptionsConsume),
                   MODULE(TcpUdpModify),
                   MODULE(IPOptionsModify),
               },
               TargetMemoryBank_ptr(new BMv2MemoryBank())) {}
    
    static Target_ptr build() { return Target_ptr(new BMv2Target()); }
};

} // namespace bmv2
} // namespace targets
} // namespace synapse
