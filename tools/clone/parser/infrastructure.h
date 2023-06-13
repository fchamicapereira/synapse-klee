#pragma once

#include <list>
#include <string>

#include "../models/device.h"
#include "../models/link.h"
#include "../models/port.h"


namespace Clone {
	using std::string;
	using std::list;

	class Infrastructure {
	protected:
		const DeviceMap devices;
		const LinkList links;
		const PortMap ports;

		Infrastructure(DeviceMap&& devices, LinkList&& links, PortMap&& ports);
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

		void print() const;
	};
}