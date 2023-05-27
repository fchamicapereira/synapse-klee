#pragma once

#include "../models/device.hpp"
#include "../models/link.hpp"
#include "../models/port.hpp"


namespace Clone {
	class Infrastructure {
	protected:
		const DeviceMap devices;
		const LinkList links;
		const PortMap ports;

		Infrastructure(DeviceMap&& devices, LinkList&& links, PortMap&& ports);
	public:
		static unique_ptr<Infrastructure> create(DeviceMap&& devices, LinkList&& links, PortMap&& ports);

		void print();
	};
}
