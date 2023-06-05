#include "infrastructure.h"
#include "../pch.h"
#include "klee/util/ExprHashMap.h"

#include <unordered_set>

namespace Clone {
	using std::unordered_set;

	Infrastructure::Infrastructure(DeviceMap&& devices, LinkList&& links, PortMap&& ports) : devices(move(devices)), links(move(links)), ports(move(ports)) {}

	unique_ptr<Infrastructure> Infrastructure::create(DeviceMap&& devices, LinkList&& links, PortMap&& ports) {
		if(devices.size() == 0) danger("No devices found");
		if(links.size() == 0) danger("No links found");
		if(ports.size() == 0) danger("No ports found");

		return unique_ptr<Infrastructure>(new Infrastructure(move(devices), move(links), move(ports)));
	}

	const list<string> Infrastructure::get_device_types() const {
		unordered_set<string> s;

		for(auto it = devices.begin(); it != devices.end(); ++it) {
			s.insert(it->second->get_type());
		}

		return list<string>(s.begin(), s.end());
	}

	void Infrastructure::print() const {
		debug("Printing Network");

		for (auto it = devices.begin(); it != devices.end(); ++it) {
			it->second->print();
		}

		for (const auto& link: links) {
			link->print();
		}

		for (auto it = ports.begin(); it != ports.end(); ++it) {
			it->second->print();
		}
	}
}