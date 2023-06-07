#pragma once

#include "../../target.h"
#include "../module.h"

#include "memory_bank.h"
#include "if.h"
#include "then.h"
#include "else.h"
#include "drop.h"


namespace synapse {
namespace targets {
namespace clone {

class CloneTarget : public Target {
public:
	CloneTarget()
		: Target(TargetType::CloNe,
				 {
					MODULE(If),
					MODULE(Then),
					MODULE(Else),	
					MODULE(Drop)
				 },
				 TargetMemoryBank_ptr(new CloneMemoryBank())) {}

	static Target_ptr build() { 
		return Target_ptr(new CloneTarget()); 
	}
};

} // namespace clone
} // namespace targets
} // namespace synapse