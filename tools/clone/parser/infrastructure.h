#pragma once

#include <list>
#include <string>
#include <map>

#include "../models/device.h"
#include "../models/link.h"
#include "../models/port.h"


namespace Clone {
	using std::string;
	using std::list;
	using std::map;

	struct Graph;
	typedef shared_ptr<Graph> GraphPtr;

	class Infrastructure {
	protected:
		const DeviceMap devices;
		const LinkList links;
		const PortMap ports;
		GraphPtr graph;

		map<string, map<string, string>> m_routing_table; // source < target, next >

		Infrastructure(DeviceMap&& devices, LinkList&& links, PortMap&& ports);

		map<string, string> dijkstra(DevicePtr device);
	public:
		static unique_ptr<Infrastructure> create(DeviceMap&& devices, LinkList&& links, PortMap&& ports);

		const list<string> get_device_types() const;

		inline const PortPtr& get_port(unsigned port) const {
			return ports.at(port);
		}

		inline const PortMap& get_ports() const {
			return ports;
		}

		inline const DeviceMap& get_devices() {
			return devices;
		}

		inline const DevicePtr& get_device(const string& name) const {
			return devices.at(name);
		}

		void build_routing_table();

		inline const map<string, string>& get_routing_table(const string& source) {
			return m_routing_table.at(source);
		}

		void print() const;
	};
}