#pragma once

#include <map>
#include <vector>
#include <memory>

#include "../models/device.h"
#include "../models/link.h"

namespace Clone {
	using std::map;
	using std::vector;
	using std::shared_ptr;

	struct Graph {
		map<string, map<unsigned, string>> m;

		Graph() = default;

		void init(DeviceMap devices, LinkList links) {
			for (auto it = devices.begin(); it != devices.end(); ++it) {
				m[it->first] = map<unsigned, string>();
			}

			for(auto link: links) {
				m[link->get_node1()][link->get_port1()] = link->get_node2();
				m[link->get_node2()][link->get_port2()] = link->get_node1();
			}
		}

		void print() const {
			for(auto it = m.begin(); it != m.end(); ++it) {
				debug("Node: ", it->first);
				for(auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
					debug("Port: ", it2->first, " -> ", it2->second);
				}
			}
		}
	};

	typedef shared_ptr<Graph> GraphPtr;
}