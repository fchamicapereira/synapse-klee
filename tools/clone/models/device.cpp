#include "device.h"

#include "../pch.h"


namespace Clone {
	Device::Device(string id, string type) : id(id), type(type) {}

	Device::~Device() = default;

	void Device::print() const {
		debug("Device ", id, " type ", type);
	}
}