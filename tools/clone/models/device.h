#pragma once

#include <cassert>
#include <string>
#include <unordered_map>
#include <memory>

namespace Clone {
	using std::string;
	using std::shared_ptr;
	using std::unordered_map;

	class Device {
	private:
		const string id;
		const string type;
		unordered_map<unsigned, unsigned> ports;
	public:
		Device(string id, string type = "");
		~Device();

		inline const string& get_id() const {
			return id;
		}

		inline const string& get_type() const {
			return type;
		}

		inline unsigned get_port(const unsigned local_port) const {
			assert(ports.find(local_port) != ports.end());
			return ports.at(local_port);
		}

		inline void add_port(const unsigned local_port, const unsigned global_port) {
			ports[local_port] = global_port;
		}

		void print() const;
	};
	
	typedef shared_ptr<Device> DevicePtr;
	typedef unordered_map<string, DevicePtr> DeviceMap;
}