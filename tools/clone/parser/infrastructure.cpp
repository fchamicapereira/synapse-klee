#include "infrastructure.h"
#include "../pch.h"
#include "graph.h"

#include <unordered_set>
#include <map>
#include <string>

namespace Clone {
	using std::unordered_set;
	using std::map;
	using std::string;

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

	map<string, string> Infrastructure::dijkstra(DevicePtr device) {
		map<string, unsigned> distances;
		map<string, string> previous;
		unordered_set<string> q;

		for(auto it = devices.begin(); it != devices.end(); ++it) {
			q.insert(it->first);
			distances[it->first] = -1;
			q.insert(it->first);
		}

		distances[device->get_id()] = 0;

		while(q.size() > 0) {
			string min_dev = "";
			unsigned min_val = -1;
			for(const string& s: q) {
				unsigned val = distances[s];
				if(val < min_val) {
					min_val = val;
					min_dev = s;
				}
			}

			q.erase(min_dev);
			auto neighbours = graph->m[min_dev];

			for(auto neighbour: neighbours) {
				unsigned alt = distances[min_dev] + 1;
				if(alt < distances[neighbour.second]) {
					distances[neighbour.second] = alt;
					previous[neighbour.second] = min_dev;
				}
			}
		}

		map<string, string> routes;
		distances.erase(device->get_id());
		routes[device->get_id()] = device->get_id();

		for(auto distance: distances) {
			auto curr = distance.first;
			auto prev = previous[curr];

			while(prev != device->get_id()) {
				curr = prev;
				prev = previous[curr];
			}

			routes[distance.first] = curr;
		}

		return routes;
	}

	void Infrastructure::build_routing_table() {
		GraphPtr graph = GraphPtr(new Graph());
		graph->init(devices, links);
		this->graph = graph;

		for(auto it = devices.begin(); it != devices.end(); ++it) {
			auto m = dijkstra(it->second);
			m_routing_table[it->first] = m;
		}
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