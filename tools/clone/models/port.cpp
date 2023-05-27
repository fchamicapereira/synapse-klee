#include "port.hpp"
#include "../pch.hpp"
#include "device.hpp"

namespace Clone {
	Port::Port(const DevicePtr device, unsigned device_port, unsigned global_port): 
		device(device), device_port(device_port), global_port(global_port) {}

	Port::~Port() = default;

	void Port::print() const {
		debug("Global port ", global_port, " connected to device (", device->get_id(), ") with local port ", device_port);
	}
}