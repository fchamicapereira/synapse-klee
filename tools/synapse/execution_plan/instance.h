#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace synapse {

struct Instance {
	const std::string name; 

	Instance(const std::string &_name) : name(_name) {}
};

typedef std::shared_ptr<Instance> Instance_ptr;

} // namespace synapse