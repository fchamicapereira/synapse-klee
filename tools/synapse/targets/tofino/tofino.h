#pragma once

#include "../target.h"

#include "ignore.h"
#include "if.h"
#include "if_header_valid.h"
#include "then.h"
#include "else.h"
#include "forward.h"
#include "drop.h"
#include "broadcast.h"
#include "parse_header.h"
#include "modify_header.h"

#include "tofino_context.h"

namespace synapse {
namespace tofino {

struct TofinoTarget : public Target {
  TofinoTarget(TNAVersion version)
      : Target(TargetType::Tofino,
               {
                   new IgnoreGenerator(),
                   new IfGenerator(),
                   new IfHeaderValidGenerator(),
                   new ThenGenerator(),
                   new ElseGenerator(),
                   new ForwardGenerator(),
                   new DropGenerator(),
                   new BroadcastGenerator(),
                   new ParseHeaderGenerator(),
                   new ModifyHeaderGenerator(),
               },
               new TofinoContext(version)) {}
};

} // namespace tofino
} // namespace synapse
