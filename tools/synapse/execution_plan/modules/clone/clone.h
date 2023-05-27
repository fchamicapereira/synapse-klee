#pragma once

#include "../../target.h"
#include "../module.h"

#include "memory_bank.h"

namespace synapse {
namespace targets {
namespace clone {

class CloneTarget : public Target {
public:
	CloneTarget()
		: Target(TargetType::CloNe,
				 {},
				 MemoryBank_ptr(new CloneMemoryBank())) {}

	static Target_ptr build() { return Target_ptr(new CloneTarget()); }
};

} // namespace clone
} // namespace targets
} // namespace synapse
