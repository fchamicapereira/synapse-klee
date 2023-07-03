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
	CloneTarget(Instance_ptr _instance)
		: Target(TargetType::CloNe,
				 {
					MODULE(If),
					MODULE(Then),
					MODULE(Else),	
					MODULE(Drop)
				 },
				 TargetMemoryBank_ptr(new CloneMemoryBank()),
				 _instance) {}

	static Target_ptr build(Instance_ptr _instance = nullptr) { 
		return Target_ptr(new CloneTarget(_instance)); 
	}
};

} // namespace clone
} // namespace targets
} // namespace synapse